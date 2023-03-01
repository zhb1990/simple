#include "center.h"

#include <gate_connector.h>
#include <google/protobuf/util/json_util.h>
#include <msg_ec.pb.h>
#include <msg_id.pb.h>
#include <msg_server.pb.h>
#include <simple/application/frame_awaiter.h>
#include <simple/coro/timed_awaiter.h>
#include <simple/log/log.h>
#include <simple/shm/shm.h>
#include <simple/utils/os.h>

#include <ctime>
#include <simple/coro/co_start.hpp>
#include <simple/coro/task_operators.hpp>
#include <stdexcept>

constexpr std::string_view shm_account_name = "shm_account_infos";
// 随意定义的数量
constexpr size_t shm_account_size = 1024;
constexpr size_t shm_account_bytes = sizeof(account_info) * shm_account_size;

center::center(const simple::toml_value_t* value) {
    if (!value->is_table()) {
        throw std::logic_error("center need args");
    }

    auto& args = value->as_table();
    if (const auto it = args.find("gate"); it != args.end()) {
        gate_connector_ = std::make_shared<gate_connector>(
            *this, &it->second, game::st_center, [this] { return on_register_to_gate(); },
            [this](uint16_t from, uint64_t session, uint16_t id, const simple::memory_buffer& buffer) {
                return forward_shm(from, session, id, buffer);
            },
            gate_connector::shm_infos{{shm_account_name.data(), static_cast<uint32_t>(shm_account_bytes)}});
    } else {
        throw std::logic_error("center need gate config");
    }

    restore_shm();
}

center::~center() noexcept = default;

simple::task<> center::awake() {
    gate_connector_->start();
    simple::co_start([this] { return check_account_info(); });
    simple::co_start([this] { return clear_not_found(); });
    co_return;
}

void center::restore_shm() {
    shm_ = std::make_unique<simple::shm>(shm_account_name, shm_account_bytes);
    auto* data = static_cast<account_info*>(shm_->data());
    if (shm_->is_create()) {
        memset(data, 0, shm_account_bytes);
        for (size_t i = 0; i < shm_account_size; ++i) {
            empty_info_.emplace_back(data + i);
        }
    } else {
        for (size_t i = 0; i < shm_account_size; ++i) {
            auto* temp = data + i;
            if (temp->userid == 0) {
                empty_info_.emplace_back(temp);
                continue;
            }

            // 有数据的，还原 userid_to_info_ 与 account_to_info_
            userid_to_info_[temp->userid].info = temp;
            account_to_info_[temp->account].info = temp;
        }
    }
}

simple::task<> center::on_register_to_gate() { return gate_connector_->subscribe(game::st_db_proxy); }

void center::forward_shm(uint16_t from, uint64_t session, uint16_t id, const simple::memory_buffer& buffer) {}

#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
simple::task<> center::check_account_info() {
    //    game::s_db_update_account_brd brd;
    // 每次更新发送的数量
    constexpr size_t bulk_write_max = 1000;
    // 超过1小时的删除
    constexpr int64_t timeout_delete = 3600;
    auto* data = static_cast<account_info*>(shm_->data());

    for (;;) {
        //        brd.clear_infos();
        co_await simple::sleep_for(std::chrono::seconds(60));

        auto now = time(nullptr);
        for (size_t i = 0; i < shm_account_size; ++i) {
            // 每帧处理256个
            if ((i & 0xff) == 0xff) {
                co_await simple::skip_frame(1);
                now = time(nullptr);
            }

            auto* temp = data + i;
            if (temp->userid == 0) {
                continue;
            }

            if (temp->version != temp->version_db) {
                //                updates.emplace_back(temp);
                //                if (brd.infos_size() >= bulk_write_max) {
                //                    // 发送更新
                //
                //                }
                continue;
            }

            if (now - temp->last_time < timeout_delete) {
                continue;
            }

            // 删除映射关系
            userid_to_info_.erase(temp->userid);
            account_to_info_.erase(temp->account);

            // 重置放入未使用的队列
            memset(temp, 0, sizeof(*temp));
            empty_info_.emplace_back(temp);
        }

        // 发送更新
    }
}
#pragma clang diagnostic pop

simple::task<> center::clear_not_found() {
    for (;;) {
        co_await simple::sleep_for(std::chrono::hours(12));
        userid_not_found_.clear();
        account_not_found_.clear();
    }
}

SIMPLE_SERVICE_API simple::service_base* center_create(const simple::toml_value_t* value) { return new center(value); }

SIMPLE_SERVICE_API void center_release(const simple::service_base* t) { delete t; }
