#pragma once

#include <simple/coro/condition_variable.h>

#include <deque>
#include <simple/application/service.hpp>
#include <simple/containers/buffer.hpp>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace simple {
class shm;
}

class gate_connector;

// 使用boost::interprocess 加 boost::multi_index::multi_index_container会简单很多
// 先不使用boost，用最原始的方式
struct account_info {
    // 上次访问的时间(除了登录、改名、改密码，如果玩家在线，逻辑服务也会定时来刷新这个时间)
    int64_t last_time;
    // 当前数据的版本号
    uint16_t version;
    // 已经写入db的版本号
    uint16_t version_db;
    // 玩家id
    int32_t userid;
    // 账号名
    char account[64];
    // 密码
    char password[64];
};

class center final : public simple::service_base {
  public:
    explicit center(const simple::toml_value_t* value);

    SIMPLE_NON_COPYABLE(center)

    ~center() noexcept override;

    simple::task<> awake() override;

  private:
    // 还原共享内存相关的数据
    void restore_shm();

    simple::task<> on_register_to_gate();

    simple::task<> check_account_info();

    simple::task<> clear_not_found();

    void forward_shm(uint16_t from, uint64_t session, uint16_t id, const simple::memory_buffer& buffer);

    // 处理注册、登录

    // 连接gate
    std::shared_ptr<gate_connector> gate_connector_;
    simple::memory_buffer temp_buffer_;

    // 玩家账号的共享内存
    std::unique_ptr<simple::shm> shm_;

    struct userid_key_info {
        // 异步加载时，如果有同样的userid查询，需要等待这个条件变量
        // （或者都发起查询，以最先收到的为准）
        simple::condition_variable cv;
        account_info* info{nullptr};
    };

    struct account_key_info {
        // 同 userid_key_info
        simple::condition_variable cv;
        account_info* info{nullptr};
    };

    // 根据userid 找到账号信息指针
    std::unordered_map<int32_t, userid_key_info> userid_to_info_;
    // 根据账号名 找到账号信息指针
    std::unordered_map<std::string_view, account_key_info> account_to_info_;
    // 空闲的账号信息指针
    std::deque<account_info*> empty_info_;
    // 将不存在的缓存下来，每隔12小时全部清空，不针对每项单独处理了
    std::unordered_set<int64_t> userid_not_found_;
    std::unordered_set<std::string> account_not_found_;
};
