#pragma once

#include <proto_utils.h>
#include <simple/coro/condition_variable.h>
#include <simple/web/websocket.h>

#include <simple/application/service.hpp>
#include <simple/containers/buffer.hpp>
#include <string>

constexpr uint32_t checkerboard_size = 15;

class client final : public simple::service_base {
  public:
    simple::task<> awake() override;

    static simple::task<std::string> cin();

    template <std::derived_from<google::protobuf::Message> Message>
    simple::task<Message> call(uint16_t id, const google::protobuf::Message& req) {
        const auto session = system_.create_session();
        simple::memory_buffer buf;
        init_client_buffer(buf, id, session, req);
        ws_.write(simple::websocket_opcode::binary, buf.begin_read(), buf.readable());
        co_return co_await system_.get_awaiter<Message>(session);
    }

  private:
    simple::task<> run();

    simple::task<> auto_ping(const simple::websocket& ws);

    simple::task<> ping();

    simple::task<> forward_message(uint64_t session, uint16_t id, const simple::memory_buffer& buffer);

    simple::task<> logic(const simple::websocket& ws);

    simple::task<> next_game();

    // 登录、注册
    simple::task<> login();
    // 匹配
    simple::task<> match();
    // 进入房间
    simple::task<> enter_room();
    // 落子
    simple::task<> move(uint32_t x, uint32_t y);

    void show_game_result(int32_t result);

    void move(bool is_black, uint32_t x, uint32_t y);

    // 本地校验下落子的坐标
    bool check_pos(uint32_t x, uint32_t y);

    void back(uint32_t x, uint32_t y);

    void show();

    std::string host_;
    std::string port_;
    std::string account_;
    std::string password_;
    std::string opponent_;
    int32_t userid_{0};
    bool has_match_{false};
    bool is_my_turn_{false};
    bool is_black_{false};

    enum class pos_state : uint8_t { none, black, white };

    // 15 * 15 的棋盘
    uint8_t checkerboard_[checkerboard_size * checkerboard_size]{};
    simple::websocket ws_{simple::websocket_type::client, 0};
    rpc_system system_;
    simple::condition_variable cv_turn_;
};
