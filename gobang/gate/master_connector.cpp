#include "master_connector.h"

#include <google/protobuf/util/json_util.h>
#include <msg_ec.pb.h>
#include <msg_id.pb.h>
#include <msg_server.pb.h>
#include <proto_utils.h>
#include <simple/coro/network.h>
#include <simple/coro/timed_awaiter.h>
#include <simple/log/log.h>
#include <simple/utils/os.h>

#include <simple/application/service.hpp>
#include <simple/coro/co_start.hpp>
#include <simple/coro/task_operators.hpp>
#include <vector>

master_connector::master_connector(simple::service& s, const simple::toml_table_t& table) : service_(s) {
    if (const auto it = table.find("master_address"); it != table.end() && it->second.is_string()) {
        master_address_ = it->second.as_string();
    } else {
        throw std::logic_error("gate need master address");
    }

    if (const auto it = table.find("remote_port"); it != table.end() && it->second.is_integer()) {
        remote_port_ = static_cast<uint16_t>(it->second.as_integer());
    } else {
        throw std::logic_error("gate need remote listen port");
    }

    if (const auto it = table.find("remote_hosts"); it != table.end() && it->second.is_array()) {
        auto& arr = it->second.as_array();
        for (auto& host : arr) {
            if (host.is_string()) {
                remote_hosts_.emplace_back(host.as_string());
            }
        }
    }

    if (remote_hosts_.empty()) {
        throw std::logic_error("gate need remote hosts");
    }

    service_.events().register_handler<service_update_event>(&master_connector::update_service, this);
    service_.router().register_call("upload_to_master", &master_connector::upload_to_master, this);
}

void master_connector::start() {
    simple::co_start([this]() { return run(); });
}

simple::task<> master_connector::run() {
    using namespace std::chrono_literals;
    auto& network = simple::network::instance();
    size_t cnt_fail = 0;
    for (;;) {
        simple::warn("[{}] address:{}", service_.name(), master_address_);
        const auto pos = master_address_.find(',');
        if (pos == std::string::npos || pos + 1 == master_address_.size()) {
            throw std::logic_error("gate master address is invalid");
        }

        const auto host = master_address_.substr(0, pos);
        const auto port = master_address_.substr(pos + 1);

        try {
            if (const auto interval = connect_interval(cnt_fail); interval > 0) {
                co_await simple::sleep_for(std::chrono::milliseconds(interval));
            }

            socket_ = co_await network.tcp_connect(host, port, 10s);
            if (socket_ == 0) {
                ++cnt_fail;
                simple::error("[{}] tcp_connect fail", service_.name());
                continue;
            }

            simple::warn("[{}] connect gate master address:{} succ.", service_.name(), master_address_);

            // 开启接收注册的结果协程
            simple::co_start([this, socket = socket_]() { return recv_one_message(socket); });

            // register
            auto timeout = []() -> simple::task<> { co_await simple::sleep_for(5s); };
            auto result = co_await (register_to_master(socket_) || timeout());
            if (const auto* succ = std::get_if<0>(&result); (!succ) || (!(*succ))) {
                if (succ) {
                    simple::error("[{}] register to gate master fail.", service_.name());
                } else {
                    // 注册超时了
                    simple::error("[{}] register to gate master timeout.", service_.name());
                }
                network.close(socket_);
                socket_ = 0;
                ++cnt_fail;
                continue;
            }

            simple::warn("[{}] register to gate master succ.", service_.name());
            cnt_fail = 0;
            // 开启ping协程
            simple::co_start([this, socket = socket_]() { return auto_ping(socket); });

            // 循环接收消息
            simple::memory_buffer buffer;
            net_header header{};
            for (;;) {
                co_await recv_net_buffer(buffer, header, socket_);
                const uint32_t len = header.len;
                simple::info("[{}] socket:{} recv id:{} session:{} len:{}", service_.name(), socket_, header.id, header.session,
                             len);
                if ((header.id & game::msg_mask) != game::msg_s2s_ack || header.session == 0 ||
                    (!system_.wake_up_session(header.session, std::string_view(buffer)))) {
                    // 不是rpc调用，分发消息
                    forward_message(header.id, buffer);
                }
            }
        } catch (std::exception& e) {
            network.close(socket_);
            socket_ = 0;
            ++cnt_fail;
            simple::error("[{}] exception {}", service_.name(), ERROR_CODE_MESSAGE(e.what()));
        }
    }
}

simple::task<> master_connector::recv_one_message(uint32_t socket) {
    simple::memory_buffer buffer;
    net_header header{};
    co_await recv_net_buffer(buffer, header, socket);
    const uint32_t len = header.len;
    simple::info("[{}] socket:{} recv id:{} session:{} len:{}", service_.name(), socket, header.id, header.session, len);
    // 第一个消息是注册的回复，一定是rpc调用
    system_.wake_up_session(header.session, std::string_view(buffer));
}

simple::task<> master_connector::auto_ping(uint32_t socket) {
    auto& network = simple::network::instance();
    while (socket == socket_) {
        co_await simple::sleep_for(std::chrono::seconds(20));
        auto timeout = []() -> simple::task<> { co_await simple::sleep_for(std::chrono::seconds(5)); };
        auto result = co_await (ping_to_master(socket) || timeout());
        if (result.index() == 0) {
            continue;
        }

        if (socket == socket_) {
            network.close(socket);
        }
        break;
    }
}

simple::task<bool> master_connector::register_to_master(uint32_t socket) {
    game::s_gate_register_req req;
    auto& info = *req.mutable_info();
    info.set_id(service_.id());
    const auto port = fmt::format("{}", remote_port_);

    for (const auto& host : remote_hosts_) {
        auto& add = *info.add_addresses();
        add.set_host(host);
        add.set_port(port);
    }

    // 填入本地的服务
    auto services = service_.router().call<std::vector<service_info*>>("local_services");
    for (auto* s : services) {
        s->to_proto(*info.add_services());
    }

    const auto ack = co_await rpc_call<game::s_gate_register_ack>(system_, socket, game::id_s_gate_register_req, req);
    if (auto& result = ack.result(); result.ec() != game::ec_success) {
        co_return false;
    }

    std::string log_str;
    google::protobuf::util::MessageToJsonString(ack, &log_str);
    simple::info("[{}] register ack:{}", service_.name(), log_str);

    // 记录其他gate的信息
    for (auto& g : ack.gates()) {
        add_remote_gate(g);
    }

    co_return true;
}

void master_connector::add_remote_gate(const game::s_gate_info& info) {
    const auto gate_id = static_cast<uint16_t>(info.id());
    bool need_start = false;
    auto it = remote_gates_.find(gate_id);
    if (it == remote_gates_.end()) {
        std::tie(it, std::ignore) = remote_gates_.emplace(gate_id, remote_gate(service_, gate_id));
        need_start = true;
    }

    std::vector<std::string> addresses;
    for (auto& address : info.addresses()) {
        auto& str = addresses.emplace_back();
        str.append(address.host());
        str.push_back(',');
        str.append(address.port());
    }
    it->second.set_addresses(std::move(addresses));

    if (need_start) {
        it->second.start();
    }

    for (auto& s : info.services()) {
        it->second.add_remote_service(s);
    }
}

simple::task<> master_connector::ping_to_master(uint32_t socket) {
    const auto ping = co_await rpc_ping(system_, socket);
    simple::info("[{}] ping delay:{}ms", service_.name(), ping);
}

void master_connector::forward_message(uint16_t id, const simple::memory_buffer& buffer) {
    // 目前只需要处理master发来的其他gate信息广播，直接if判断下
    if (id != game::id_s_gate_register_brd) {
        return;
    }

    game::s_gate_register_brd brd;
    if (!brd.ParseFromArray(buffer.begin_read(), static_cast<int>(buffer.readable()))) {
        simple::warn("[{}] parse s_gate_register_brd fail", service_.name());
        return;
    }

    std::string log_str;
    google::protobuf::util::MessageToJsonString(brd, &log_str);
    simple::info("[{}] register brd:{}", service_.name(), log_str);

    // 记录其他gate的信息
    for (auto& g : brd.gates()) {
        add_remote_gate(g);
    }
}

simple::task<int32_t> master_connector::upload(const game::s_service_info& info) {
    if (socket_ == 0) {
        co_return game::ec_system;
    }

    game::s_service_update_req req;
    *req.add_services() = info;

    const auto ack = co_await rpc_call<game::msg_common_ack>(system_, socket_, game::id_s_service_update_req, req);
    co_return ack.result().ec();
}

void master_connector::update_service(const service_update_event& data) {
    if (data.info->gate != service_.id()) {
        return;
    }

    simple::co_start([this, service = data.info]() {
        game::s_service_info info;
        service->to_proto(info);
        return upload(info);
    });
}

std::shared_ptr<simple::task<int32_t>> master_connector::upload_to_master(const game::s_service_info& info) {
    return std::make_shared<simple::task<int32_t>>(upload(info));
}
