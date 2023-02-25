#include "db_proxy.h"

#include <gate_connector.h>
#include <google/protobuf/util/json_util.h>
#include <msg_ec.pb.h>
#include <msg_id.pb.h>
#include <msg_server.pb.h>
#include <simple/coro/async_session.h>
#include <simple/coro/thread_pool.h>
#include <simple/log/log.h>
#include <simple/utils/os.h>

#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/pool.hpp>
#include <mongocxx/uri.hpp>
#include <simple/coro/co_start.hpp>
#include <simple/coro/task_operators.hpp>
#include <stdexcept>
#include <string_view>

static mongocxx::instance instance{};

constexpr std::string_view db_name = "gobang";

constexpr std::string_view user_info_name = "user_info";

constexpr std::string_view account_info_name = "account_info";

constexpr std::string_view ai_list_name = "ai_list";

// 总共3个表
// 一个记录账号 密码 userid
// 一个记录 userid 是否ai 输赢次数
// 一个记录所有的ai的userid

db_proxy::db_proxy(const simple::toml_value_t* value) {
    if (!value->is_table()) {
        throw std::logic_error("db_proxy need args");
    }

    auto& args = value->as_table();
    if (const auto it = args.find("db_uri"); it != args.end() && it->second.is_string()) {
        uri_ = it->second.as_string();
    }

    if (const auto it = args.find("gate"); it != args.end()) {
        gate_connector_ = std::make_shared<gate_connector>(
            *this, &it->second, game::st_db_proxy, gate_connector::fn_on_register{},
            [this](uint16_t from, uint64_t session, uint16_t id, const simple::memory_buffer& buffer) {
                return forward_shm(from, session, id, buffer);
            });
    } else {
        throw std::logic_error("proxy need gate config");
    }

    message_callbacks_.emplace(game::id_s_db_create_user_req,
                               [this](uint16_t from, uint64_t session, const simple::memory_buffer& buffer) {
                                   return create_user(from, session, buffer);
                               });
    message_callbacks_.emplace(game::id_s_db_query_user_req,
                               [this](uint16_t from, uint64_t session, const simple::memory_buffer& buffer) {
                                   return query_user(from, session, buffer);
                               });
    message_callbacks_.emplace(
        game::id_s_db_update_user_brd,
        [this](uint16_t from, uint64_t, const simple::memory_buffer& buffer) { return update_user(from, buffer); });
    message_callbacks_.emplace(game::id_s_db_query_account_req,
                               [this](uint16_t from, uint64_t session, const simple::memory_buffer& buffer) {
                                   return query_account(from, session, buffer);
                               });
    message_callbacks_.emplace(
        game::id_s_db_update_account_brd,
        [this](uint16_t from, uint64_t, const simple::memory_buffer& buffer) { return update_account(from, buffer); });
    message_callbacks_.emplace(
        game::id_s_db_query_max_userid_req,
        [this](uint16_t from, uint64_t session, const simple::memory_buffer&) { return query_max_userid(from, session); });
    message_callbacks_.emplace(
        game::id_s_db_query_ai_req,
        [this](uint16_t from, uint64_t session, const simple::memory_buffer&) { return query_ai(from, session); });
}

db_proxy::~db_proxy() noexcept = default;

simple::task<> db_proxy::awake() {
    simple::warn("[{}] awake", name());
    if (uri_.empty()) {
        pool_ = std::make_unique<mongocxx::pool>(mongocxx::uri{});
    } else {
        pool_ = std::make_unique<mongocxx::pool>(mongocxx::uri{uri_});
    }

    // 创建索引
    simple::async_session_awaiter<void> awaiter;
    simple::thread_pool::instance().post([this, session = awaiter.get_async_session()] {
        create_index();
        session.set_result();
    });
    co_await awaiter;

    gate_connector_->start();
}

void db_proxy::create_index() {
    using bsoncxx::builder::basic::kvp;
    using bsoncxx::builder::basic::make_document;
    using namespace std::string_view_literals;

    auto en = pool_->acquire();
    auto& client = *en;
    auto db = client[db_name];

    // user_info 建立userid的索引
    auto user_info = db[user_info_name];
    bool has_index_userid = false;
    for (auto& doc : user_info.list_indexes()) {
        if (auto it = doc.find("ns"); it != doc.end() && it->get_string() == "user_info_index_userid"sv) {
            has_index_userid = true;
            break;
        }
    }
    if (!has_index_userid) {
        mongocxx::options::index index;
        index.unique(true);
        index.name("user_info_index_userid");
        user_info.create_index(make_document(kvp("userid", 1)), index);
    }

    // account_info 分别建立 userid的索引和account的索引
    auto account_info = db[account_info_name];
    has_index_userid = false;
    bool has_index_account = false;
    for (auto& doc : account_info.list_indexes()) {
        auto it = doc.find("ns");
        if (it == doc.end()) {
            continue;
        }

        if (const std::string_view name = it->get_string(); name == "account_info_index_userid"sv) {
            has_index_userid = true;
            if (has_index_account) {
                break;
            }
        } else if (name == "account_info_index_account"sv) {
            has_index_account = true;
            if (has_index_userid) {
                break;
            }
        }
    }
    if (!has_index_userid) {
        mongocxx::options::index index;
        index.unique(true);
        index.name("account_info_index_userid");
        account_info.create_index(make_document(kvp("userid", 1)), index);
    }
    if (!has_index_account) {
        mongocxx::options::index index;
        index.unique(true);
        index.name("account_info_index_account");
        account_info.create_index(make_document(kvp("account", 1)), index);
    }
}

void db_proxy::forward_shm(uint16_t from, uint64_t session, uint16_t id, const simple::memory_buffer& buffer) {
    if (const auto it = message_callbacks_.find(id); it != message_callbacks_.end()) {
        simple::co_start([from, session, buffer, &fn = it->second]() { return fn(from, session, buffer); });
    }
}

simple::task<> db_proxy::create_user(uint16_t from, uint64_t session, const simple::memory_buffer& buffer) {
    using bsoncxx::builder::basic::kvp;
    using bsoncxx::builder::basic::make_document;
    game::msg_common_ack ack;
    game::s_db_create_user_req req;
    if (!req.ParseFromArray(buffer.begin_read(), static_cast<int>(buffer.readable()))) {
        ack.mutable_result()->set_ec(game::ec_system);
        gate_connector_->write(from, session, game::id_s_db_create_user_ack, ack);
        co_return;
    }

    try {
        simple::async_session_awaiter<void> awaiter;
        simple::thread_pool::instance().post([session = awaiter.get_async_session(), this, &req] {
            try {
                auto en = pool_->acquire();
                auto db = (*en)[db_name];
                auto doc = make_document(kvp("userid", req.userid()), kvp("win_count", req.win_count()),
                                         kvp("lose_count", req.lose_count()), kvp("is_ai", req.is_ai()));
                // 不去判断返回值了
                db[user_info_name].insert_one(doc.view());

                doc =
                    make_document(kvp("userid", req.userid()), kvp("account", req.account()), kvp("password", req.password()));
                // 不去判断返回值了
                db[account_info_name].insert_one(doc.view());

                if (req.is_ai()) {
                    db[ai_list_name].insert_one(make_document(kvp("userid", req.userid())));
                }

                session.set_result();
            } catch (...) {
                session.set_exception(std::current_exception());
            }
        });
        co_await awaiter;
        ack.mutable_result()->set_ec(game::ec_success);
    } catch (std::exception& e) {
        ack.mutable_result()->set_ec(game::ec_system);
        simple::error("[{}] create user:{} exception {}", name(), req.userid(), ERROR_CODE_MESSAGE(e.what()));
    } catch (...) {
        simple::error("[{}] create user:{} unknown exception", name(), req.userid());
        ack.mutable_result()->set_ec(game::ec_system);
    }

    gate_connector_->write(from, session, game::id_s_db_create_user_ack, ack);
}

simple::task<> db_proxy::query_user(uint16_t from, uint64_t session, const simple::memory_buffer& buffer) {
    using bsoncxx::builder::basic::kvp;
    using bsoncxx::builder::basic::make_document;
    game::s_db_query_user_ack ack;
    game::s_db_query_user_req req;
    if (!req.ParseFromArray(buffer.begin_read(), static_cast<int>(buffer.readable()))) {
        ack.mutable_result()->set_ec(game::ec_system);
        gate_connector_->write(from, session, game::id_s_db_query_user_ack, ack);
        co_return;
    }

    const auto userid = req.userid();

    try {
        simple::async_session_awaiter<void> awaiter;
        simple::thread_pool::instance().post([session = awaiter.get_async_session(), this, userid, &ack] {
            try {
                auto en = pool_->acquire();
                auto coll = (*en)[db_name][user_info_name];
                auto result = coll.find_one(make_document(kvp("userid", userid)));
                if (!result.has_value()) {
                    ack.mutable_result()->set_ec(game::ec_no_data);
                    return session.set_result();
                }

                auto& doc = result.value();
                ack.set_userid(userid);
                ack.set_is_ai(doc["userid"].get_int32());
                ack.set_win_count(doc["win_count"].get_int32());
                ack.set_lose_count(doc["lose_count"].get_int32());
                session.set_result();
            } catch (...) {
                session.set_exception(std::current_exception());
            }
        });
        co_await awaiter;
        ack.mutable_result()->set_ec(game::ec_success);
    } catch (std::exception& e) {
        ack.mutable_result()->set_ec(game::ec_system);
        simple::error("[{}] query user:{} exception {}", name(), userid, ERROR_CODE_MESSAGE(e.what()));
    } catch (...) {
        simple::error("[{}] query user:{} unknown exception", name(), userid);
        ack.mutable_result()->set_ec(game::ec_system);
    }

    gate_connector_->write(from, session, game::id_s_db_query_user_ack, ack);
}

simple::task<> db_proxy::update_user(uint16_t from, const simple::memory_buffer& buffer) {
    game::s_db_update_user_brd input;
    if (!input.ParseFromArray(buffer.begin_read(), static_cast<int>(buffer.readable()))) {
        co_return;
    }

    std::vector<user_infos::iterator> infos;
    for (const auto& user : input.infos()) {
        const auto userid = user.userid();
        auto it = user_infos_.find(userid);
        if (it != user_infos_.end()) {
            // 说明还没更新完成，加锁修改
            std::scoped_lock lock{*it->mtx};
            it->version = static_cast<uint16_t>(user.version());
            it->win_count = user.win_count();
            it->lose_count = user.lose_count();
            continue;
        }

        std::tie(it, std::ignore) = user_infos_.emplace(user_info{.userid = userid});
        it->mtx = std::make_unique<std::mutex>();
        it->version = static_cast<uint16_t>(user.version());
        it->win_count = user.win_count();
        it->lose_count = user.lose_count();

        infos.emplace_back(it);
    }

    if (infos.empty()) {
        co_return;
    }

    game::s_db_update_user_brd output;
    try {
        simple::async_session_awaiter<void> awaiter;
        simple::thread_pool::instance().post([session = awaiter.get_async_session(), this, &infos, &output] {
            using bsoncxx::builder::basic::kvp;
            using bsoncxx::builder::basic::make_document;
            using mongocxx::model::update_one;
            try {
                auto en = pool_->acquire();
                auto coll = (*en)[db_name][user_info_name];
                auto bulk = coll.create_bulk_write();
                for (auto& it : infos) {
                    auto* info = output.add_infos();
                    info->set_userid(it->userid);

                    std::unique_lock lock{*it->mtx};
                    update_one upsert_op{make_document(kvp("userid", it->userid)),
                                         make_document(kvp("$set", make_document(kvp("win_count", it->win_count),
                                                                                 kvp("lose_count", it->lose_count))))};
                    info->set_version(it->version);
                    lock.unlock();
                    bulk.append(upsert_op);
                }

                // 不判断返回值了
                bulk.execute();
                session.set_result();
            } catch (...) {
                session.set_exception(std::current_exception());
            }
        });
        co_await awaiter;
    } catch (std::exception& e) {
        output.Clear();
        simple::error("[{}] update user exception {}", name(), ERROR_CODE_MESSAGE(e.what()));
    } catch (...) {
        output.Clear();
        simple::error("[{}] update user unknown exception", name());
    }

    // 删除已经更新完的，不检查是否更新到最新版本，由逻辑服务定时更新有改变的
    for (auto& it : infos) {
        user_infos_.erase(it);
    }

    if (output.infos_size() > 0) {
        // 通知已经更新到的版本
        gate_connector_->write(from, 0, game::id_s_db_update_user_brd, output);
    }
}

simple::task<> db_proxy::query_account(uint16_t from, uint64_t session, const simple::memory_buffer& buffer) {
    using bsoncxx::builder::basic::kvp;
    using bsoncxx::builder::basic::make_document;
    game::s_db_query_account_ack ack;
    game::s_db_query_account_req req;
    if (!req.ParseFromArray(buffer.begin_read(), static_cast<int>(buffer.readable()))) {
        ack.mutable_result()->set_ec(game::ec_system);
        gate_connector_->write(from, session, game::id_s_db_query_account_ack, ack);
        co_return;
    }

    const auto userid = req.userid();
    const auto& account = req.account();

    try {
        simple::async_session_awaiter<void> awaiter;
        simple::thread_pool::instance().post([session = awaiter.get_async_session(), this, userid, &account, &ack] {
            try {
                auto en = pool_->acquire();
                auto coll = (*en)[db_name][account_info_name];
                std::optional<bsoncxx::document::value> result;
                if (account.empty()) {
                    result = coll.find_one(make_document(kvp("userid", userid)));
                } else {
                    result = coll.find_one(make_document(kvp("account", account)));
                }

                if (!result.has_value()) {
                    ack.mutable_result()->set_ec(game::ec_no_data);
                    return session.set_result();
                }

                auto& doc = result.value();
                ack.set_userid(doc["userid"].get_int32());
                ack.mutable_account()->assign(doc["account"].get_string());
                ack.mutable_password()->assign(doc["password"].get_string());
                session.set_result();
            } catch (...) {
                session.set_exception(std::current_exception());
            }
        });
        co_await awaiter;
        ack.mutable_result()->set_ec(game::ec_success);
    } catch (std::exception& e) {
        ack.mutable_result()->set_ec(game::ec_system);
        simple::error("[{}] query account:{} userid:{} exception {}", name(), account, userid, ERROR_CODE_MESSAGE(e.what()));
    } catch (...) {
        simple::error("[{}] query account:{} userid:{} unknown exception", name(), account, userid);
        ack.mutable_result()->set_ec(game::ec_system);
    }

    gate_connector_->write(from, session, game::id_s_db_query_account_ack, ack);
}

simple::task<> db_proxy::update_account(uint16_t from, const simple::memory_buffer& buffer) {
    game::s_db_update_account_brd input;
    if (!input.ParseFromArray(buffer.begin_read(), static_cast<int>(buffer.readable()))) {
        co_return;
    }

    std::vector<account_infos::iterator> infos;
    for (const auto& info : input.infos()) {
        const auto userid = info.userid();
        auto it = account_infos_.find(userid);
        if (it != account_infos_.end()) {
            // 说明还没更新完成，加锁修改
            std::scoped_lock lock{*it->mtx};
            it->version = static_cast<uint16_t>(info.version());
            it->account = info.account();
            it->password = info.password();
            continue;
        }

        std::tie(it, std::ignore) = account_infos_.emplace(account_info{.userid = userid});
        it->mtx = std::make_unique<std::mutex>();
        it->version = static_cast<uint16_t>(info.version());
        it->account = info.account();
        it->password = info.password();

        infos.emplace_back(it);
    }

    if (infos.empty()) {
        co_return;
    }

    game::s_db_update_account_brd output;
    try {
        simple::async_session_awaiter<void> awaiter;
        simple::thread_pool::instance().post([session = awaiter.get_async_session(), this, &infos, &output] {
            using bsoncxx::builder::basic::kvp;
            using bsoncxx::builder::basic::make_document;
            using mongocxx::model::update_one;
            try {
                auto en = pool_->acquire();
                auto coll = (*en)[db_name][account_info_name];
                auto bulk = coll.create_bulk_write();
                for (auto& it : infos) {
                    auto* info = output.add_infos();
                    info->set_userid(it->userid);

                    std::unique_lock lock{*it->mtx};
                    update_one upsert_op{
                        make_document(kvp("userid", it->userid)),
                        make_document(kvp("$set", make_document(kvp("account", it->account), kvp("password", it->password))))};
                    info->set_version(it->version);
                    lock.unlock();
                    bulk.append(upsert_op);
                }

                // 不判断返回值了
                bulk.execute();
                session.set_result();
            } catch (...) {
                session.set_exception(std::current_exception());
            }
        });
        co_await awaiter;
    } catch (std::exception& e) {
        output.Clear();
        simple::error("[{}] update account exception {}", name(), ERROR_CODE_MESSAGE(e.what()));
    } catch (...) {
        output.Clear();
        simple::error("[{}] update account unknown exception", name());
    }

    // 删除已经更新完的，不检查是否更新到最新版本，由逻辑服务定时更新有改变的
    for (auto& it : infos) {
        account_infos_.erase(it);
    }

    if (output.infos_size() > 0) {
        // 通知已经更新到的版本
        gate_connector_->write(from, 0, game::id_s_db_update_account_brd, output);
    }
}

simple::task<> db_proxy::query_max_userid(uint16_t from, uint64_t session) {
    using bsoncxx::builder::basic::kvp;
    using bsoncxx::builder::basic::make_document;
    game::s_db_query_max_userid_ack ack;
    try {
        simple::async_session_awaiter<int32_t> awaiter;
        simple::thread_pool::instance().post([session = awaiter.get_async_session(), this] {
            try {
                auto en = pool_->acquire();
                auto coll = (*en)[db_name][account_info_name];
                mongocxx::options::find options;
                options.sort(make_document(kvp("userid", -1)));
                auto result = coll.find_one(make_document(), options);
                if (!result.has_value()) {
                    return session.set_result(0);
                }

                auto& doc = result.value();
                return session.set_result(doc["userid"].get_int32());
            } catch (...) {
                session.set_exception(std::current_exception());
            }
        });

        ack.set_max_userid(co_await awaiter);
        ack.mutable_result()->set_ec(game::ec_success);
    } catch (std::exception& e) {
        ack.mutable_result()->set_ec(game::ec_system);
        simple::error("[{}] query max userid exception {}", name(), ERROR_CODE_MESSAGE(e.what()));
    } catch (...) {
        simple::error("[{}] query max userid unknown exception", name());
        ack.mutable_result()->set_ec(game::ec_system);
    }

    gate_connector_->write(from, session, game::id_s_db_query_max_userid_ack, ack);
}

simple::task<> db_proxy::query_ai(uint16_t from, uint64_t session) {
    using bsoncxx::builder::basic::kvp;
    using bsoncxx::builder::basic::make_document;
    game::s_db_query_ai_ack ack;

    try {
        simple::async_session_awaiter<void> awaiter;
        simple::thread_pool::instance().post([session = awaiter.get_async_session(), this, &ack] {
            try {
                auto en = pool_->acquire();
                auto coll = (*en)[db_name][ai_list_name];
                auto cursor = coll.find(make_document());
                for (auto& doc : cursor) {
                    ack.add_ai_list(doc["userid"].get_int32());
                }
                ack.mutable_result()->set_ec(game::ec_success);
                return session.set_result();
            } catch (...) {
                session.set_exception(std::current_exception());
            }
        });
        co_await awaiter;
    } catch (std::exception& e) {
        ack.mutable_result()->set_ec(game::ec_system);
        simple::error("[{}] query ai exception {}", name(), ERROR_CODE_MESSAGE(e.what()));
    } catch (...) {
        simple::error("[{}] query ai unknown exception", name());
        ack.mutable_result()->set_ec(game::ec_system);
    }

    gate_connector_->write(from, session, game::id_s_db_query_ai_ack, ack);
}

SIMPLE_SERVICE_API simple::service_base* db_proxy_create(const simple::toml_value_t* value) { return new db_proxy(value); }

SIMPLE_SERVICE_API void db_proxy_release(const simple::service_base* t) { delete t; }
