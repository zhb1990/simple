﻿#include "client.h"

#include <google/protobuf/util/json_util.h>
#include <msg_base.pb.h>
#include <msg_client.pb.h>
#include <msg_ec.pb.h>
#include <msg_id.pb.h>
#include <proto_utils.h>
#include <simple/application/application.h>
#include <simple/coro/async_session.h>
#include <simple/coro/thread_pool.h>
#include <simple/coro/timed_awaiter.h>
#include <simple/log/log.h>
#include <simple/utils/os.h>
#include <simple/utils/time.h>

#include <charconv>
#include <cstdlib>
#include <iostream>
#include <simple/coro/co_start.hpp>
#include <simple/coro/task_operators.hpp>

simple::task<> client::awake() {
    simple::write_console(ERROR_CODE_MESSAGE("请输入服务器的ip:"), stdout);
    host_ = co_await cin();
    simple::write_console(ERROR_CODE_MESSAGE("请输入服务器的端口号:"), stdout);
    port_ = co_await cin();

    simple::write_console(ERROR_CODE_MESSAGE("请输入账号名字:"), stdout);
    account_ = co_await cin();
    simple::write_console(ERROR_CODE_MESSAGE("请输入账号密码:"), stdout);
    password_ = co_await cin();

    simple::co_start([this] { return run(); });
}

simple::task<std::string> client::cin() {
    simple::async_session_awaiter<std::string> awaiter;
    simple::thread_pool::instance().post([session = awaiter.get_async_session()] {
        std::string line;
        std::cin >> line;
        session.set_result(line);
    });

    co_return co_await awaiter;
}

simple::task<> client::run() {
    using namespace std::chrono_literals;
    auto& network = simple::network::instance();
    size_t cnt_fail = 0;
    simple::memory_buffer buffer;
    auto timeout = []() -> simple::task<> { co_await simple::sleep_for(5s); };
    simple::cancellation_source source;

    for (;;) {
        try {
            if (const auto interval = connect_interval(cnt_fail); interval > 0) {
                co_await simple::sleep_for(std::chrono::milliseconds(interval));
            }

            const auto socket = co_await network.tcp_connect(host_, port_, 10s);
            if (socket == 0) {
                ++cnt_fail;
                simple::error("[{}] connect server ip:{} port:{} fail", name(), host_, port_);
                continue;
            }

            ws_.socket() = socket;
            co_await ws_.handshake(host_);

            simple::warn("[{}] connect server ip:{} port:{} succ.", name(), host_, port_);
            cnt_fail = 0;

            // 连接成功时的逻辑处理
            simple::co_start([this, ws = ws_]() { return logic(ws); }, source.token());

            // 开启ping协程
            simple::co_start([this, ws = ws_]() { return auto_ping(ws); }, source.token());

            for (;;) {
                const auto op = co_await ws_.read(buffer);
                if (op == simple::websocket_opcode::close) {
                    simple::network::instance().close(socket);
                    throw std::logic_error("websocket close");
                }

                if (op == simple::websocket_opcode::ping) {
                    ws_.write(simple::websocket_opcode::pong, buffer.begin_read(), buffer.readable());
                    continue;
                }

                if (is_websocket_control(op)) {
                    // 其他控制帧，不做处理
                    continue;
                }

                const ws_header& header = *reinterpret_cast<ws_header*>(buffer.begin_read());
                buffer.read(sizeof(header));
                simple::info("[{}] socket:{} recv id:{} session:{}", name(), socket, header.id, header.session);
                if ((header.id & game::msg_mask) != game::msg_s2c_ack || header.session == 0 ||
                    (!system_.wake_up_session(header.session, std::string_view(buffer)))) {
                    // 不是rpc调用，分发消息
                    simple::co_start([header, buffer, this] { return forward_message(header.session, header.id, buffer); });
                }
            }
        } catch (std::exception& e) {
            auto& socket = ws_.socket();
            network.close(socket);
            socket = 0;
            ++cnt_fail;
            source.request_cancellation();
            source = {};
            simple::error("[{}] exception {}", name(), ERROR_CODE_MESSAGE(e.what()));
        }

        simple::write_console(ERROR_CODE_MESSAGE("网络断开，是否重连(y/n):"), stdout);
        auto line = co_await cin();
        if (line.empty() || line[0] != 'y') {
            simple::application::stop();
            co_return;
        }
    }
}

simple::task<> client::ping() {
    const auto last = simple::get_system_clock_millis();
    game::msg_empty req;
    [[maybe_unused]] const auto ack = co_await call<game::msg_common_ack>(game::id_ping_req, req);
    simple::info("[{}] ping delay:{}ms", name(), simple::get_system_clock_millis() - last);
}

simple::task<> client::auto_ping(const simple::websocket& ws) {
    auto& network = simple::network::instance();
    auto& socket = ws.socket();
    auto timeout = []() -> simple::task<> { co_await simple::sleep_for(std::chrono::seconds(30)); };
    while (socket == ws_.socket()) {
        co_await simple::sleep_for(std::chrono::seconds(30));
        if (socket != ws_.socket()) {
            break;
        }

        auto result = co_await (ping() || timeout());
        if (result.index() == 0) {
            continue;
        }

        if (socket == ws_.socket()) {
            network.close(socket);
        }
        break;
    }
}

simple::task<> client::forward_message([[maybe_unused]] uint64_t session, uint16_t id, const simple::memory_buffer& buffer) {
    // 仅处理落子的广播
    if (id != game::id_move_brd) {
        co_return;
    }

    game::move_brd brd;
    if (!brd.ParseFromArray(buffer.begin_read(), static_cast<int>(buffer.readable()))) {
        simple::warn("[{}] parse move_brd fail", name());
        co_return;
    }

    std::string log_str;
    google::protobuf::util::MessageToJsonString(brd, &log_str);
    simple::info("[{}] subscribe brd:{}", name(), log_str);

    // 落子
    move(!is_black_, brd.x(), brd.y());
    show_game_result(brd.game());
    is_my_turn_ = true;
}

simple::task<> client::logic(const simple::websocket& ws) {
    using namespace std::chrono_literals;

    try {
        // 注册或登录
        co_await login();

        if (!has_match_) {
            // 匹配
            co_await match();
        }

        // 进入棋局
        co_await enter_room();

        std::string line;
        int32_t x;
        int32_t y;
        for (;;) {
            if (!is_my_turn_) {
                co_await cv_turn_.wait();
                if (!has_match_) {
                    co_await next_game();
                }
            }

            simple::write_console(ERROR_CODE_MESSAGE("请输入落子的坐标(x,y):"), stdout);
            line = co_await cin();
            std::string_view temp = line;
            const auto pos = temp.find(',');
            if (pos == std::string_view::npos || pos + 1 == temp.size()) {
                simple::write_console(ERROR_CODE_MESSAGE("输入的坐标无效!!!            \n"), stdout);
                co_await simple::sleep_for(2s);
                continue;
            }

            auto temp1 = temp.substr(0, pos);
            std::from_chars(temp1.data(), temp1.data() + temp1.size(), x);
            temp1 = temp.substr(pos + 1);
            std::from_chars(temp1.data(), temp1.data() + temp1.size(), y);
            if (check_pos(x, y)) {
                simple::write_console(ERROR_CODE_MESSAGE("输入的坐标无效!!!            \n"), stdout);
                co_await simple::sleep_for(2s);
                continue;
            }

            // 先绘制落子
            move(is_black_, x, y);
            co_await move(x, y);

            if (!has_match_) {
                co_await next_game();
            }
        }
    } catch (std::exception& e) {
        if (auto& socket = ws.socket(); socket == ws_.socket()) {
            simple::network::instance().close(socket);
        }
        simple::error("[{}] logic exception {}", name(), ERROR_CODE_MESSAGE(e.what()));
    }
}

simple::task<> client::login() {
    game::login_req req;
    req.set_account(account_);
    req.set_password(password_);
    const auto ack = co_await call<game::login_ack>(game::id_login_req, req);
    auto& result = ack.result();
    if (const auto ec = result.ec(); ec != game::ec_success) {
        simple::error("[{}] login ec:{} {}", name(), ec, result.msg());
        // 注册失败退出
        simple::application::stop();
        co_return;
    }

    userid_ = ack.userid();
    has_match_ = ack.has_match();
}

simple::task<> client::match() {
    game::msg_empty req;
    const auto ack = co_await call<game::msg_common_ack>(game::id_match_req, req);
    auto& result = ack.result();
    if (const auto ec = result.ec(); ec != game::ec_success) {
        simple::error("[{}] match ec:{} {}", name(), ec, result.msg());
        // 匹配失败退出
        simple::application::stop();
        co_return;
    }

    has_match_ = true;
}

simple::task<> client::enter_room() {
    game::msg_empty req;
    const auto ack = co_await call<game::enter_room_ack>(game::id_enter_room_req, req);
    auto& result = ack.result();
    if (const auto ec = result.ec(); ec != game::ec_success) {
        simple::error("[{}] match ec:{} {}", name(), ec, result.msg());
        // 匹配失败退出
        simple::application::stop();
        co_return;
    }

    is_my_turn_ = ack.is_my_turn();
    is_black_ = ack.is_black();
    opponent_ = ack.opponent().account();
    // 还原棋局
    for (auto idx : ack.white()) {
        checkerboard_[idx] = static_cast<uint8_t>(pos_state::white);
    }
    for (auto idx : ack.black()) {
        checkerboard_[idx] = static_cast<uint8_t>(pos_state::black);
    }

    show();
}

simple::task<> client::move(uint32_t x, uint32_t y) {
    game::move_req req;
    req.set_x(x);
    req.set_y(y);
    const auto ack = co_await call<game::move_ack>(game::id_move_req, req);
    auto& result = ack.result();
    if (const auto ec = result.ec(); ec == game::ec_invalid_pos) {
        back(x, y);
        co_return;
    } else if (ec != game::ec_success) {
        simple::error("[{}] move ec:{} {}", name(), ec, result.msg());
        // 其他原因落子失败退出
        simple::application::stop();
        co_return;
    }

    show_game_result(ack.game());
    is_my_turn_ = false;
}

void client::show() {
    // todo: 控制台显示
}

simple::task<> client::next_game() {
    system("clear");
    simple::write_console(ERROR_CODE_MESSAGE("是否继续(y/n):"), stdout);
    const auto line = co_await cin();
    if (!line.empty() && line[0] == 'y') {
        // 匹配
        co_await match();
        // 进入棋局
        co_await enter_room();
    } else {
        simple::application::stop();
        co_return;
    }
}

void client::move(bool is_black, uint32_t x, uint32_t y) {
    // 落子
    checkerboard_[x * checkerboard_size + y] = static_cast<uint8_t>(is_black ? pos_state::black : pos_state::white);
    show();
}

void client::back(uint32_t x, uint32_t y) {
    // 回退操作
    checkerboard_[x * checkerboard_size + y] = static_cast<uint8_t>(pos_state::none);
    show();
}

bool client::check_pos(uint32_t x, uint32_t y) {
    if (x >= checkerboard_size || y >= checkerboard_size) {
        return false;
    }

    return checkerboard_[x * checkerboard_size + y] == static_cast<uint8_t>(pos_state::none);
}

void client::show_game_result(int32_t result) {
    switch (result) {
        case game::win:
            simple::write_console(ERROR_CODE_MESSAGE("------获得胜利------\n"), stdout);
            has_match_ = false;
            break;
        case game::lose:
            simple::write_console(ERROR_CODE_MESSAGE("-------失败了-------\n"), stdout);
            has_match_ = false;
            break;
        case game::draw:
            simple::write_console(ERROR_CODE_MESSAGE("--------平局--------\n"), stdout);
            has_match_ = false;
            break;
        default:
            break;
    }
    cv_turn_.notify_all();
}

SIMPLE_SERVICE_API simple::service_base* client_create(const simple::toml_value_t* value) { return new client(); }

SIMPLE_SERVICE_API void client_release(const simple::service_base* t) { delete t; }
