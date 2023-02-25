#pragma once

#include <google/protobuf/message.h>

#include <functional>
#include <mongocxx/pool.hpp>
#include <mutex>
#include <simple/application/service.hpp>
#include <simple/containers/buffer.hpp>
#include <unordered_map>
#include <unordered_set>

class gate_connector;

namespace mongocxx {
MONGOCXX_INLINE_NAMESPACE_BEGIN
class pool;
MONGOCXX_INLINE_NAMESPACE_END
}  // namespace mongocxx

struct cache_info {
    mutable uint16_t version{0};
    // 读写数据时需要加锁
    mutable std::unique_ptr<std::mutex> mtx;
};

struct account_info : cache_info {
    int32_t userid{0};
    mutable std::string account;
    mutable std::string password;

    bool operator==(const account_info& other) const { return userid == other.userid; }

    bool operator==(const int32_t& other) const { return userid == other; }
};

template <>
struct std::hash<account_info> {
    using is_transparent [[maybe_unused]] = int;

    [[nodiscard]] size_t operator()(const uint16_t& id) const noexcept { return std::hash<int32_t>()(id); }
    [[nodiscard]] size_t operator()(const account_info& data) const noexcept { return std::hash<int32_t>()(data.userid); }
};

struct user_info : cache_info {
    int32_t userid{0};
    mutable int32_t win_count{0};
    mutable int32_t lose_count{0};

    bool operator==(const user_info& other) const { return userid == other.userid; }

    bool operator==(const int32_t& other) const { return userid == other; }
};

template <>
struct std::hash<user_info> {
    using is_transparent [[maybe_unused]] = int;

    [[nodiscard]] size_t operator()(const uint16_t& id) const noexcept { return std::hash<int32_t>()(id); }
    [[nodiscard]] size_t operator()(const user_info& data) const noexcept { return std::hash<int32_t>()(data.userid); }
};

class db_proxy final : public simple::service_base {
  public:
    explicit db_proxy(const simple::toml_value_t* value);

    SIMPLE_NON_COPYABLE(db_proxy)

    ~db_proxy() noexcept override;

    simple::task<> awake() override;

  private:
    void create_index();

    void forward_shm(uint16_t from, uint64_t session, uint16_t id, const simple::memory_buffer& buffer);

    // 创建账号
    simple::task<> create_user(uint16_t from, uint64_t session, const simple::memory_buffer& buffer);

    // 根据userid 查找玩家信息
    simple::task<> query_user(uint16_t from, uint64_t session, const simple::memory_buffer& buffer);

    // 更新玩家信息
    simple::task<> update_user(uint16_t from, const simple::memory_buffer& buffer);

    // 查询玩家的账号名 密码
    simple::task<> query_account(uint16_t from, uint64_t session, const simple::memory_buffer& buffer);

    // 更新玩家的账号名 密码
    simple::task<> update_account(uint16_t from, const simple::memory_buffer& buffer);

    // 查找最大的userid
    simple::task<> query_max_userid(uint16_t from, uint64_t session);

    // 查询所有的ai的userid
    simple::task<> query_ai(uint16_t from, uint64_t session);

    // 连接gate
    std::shared_ptr<gate_connector> gate_connector_;
    // mongodb uri
    std::string uri_;
    std::unique_ptr<mongocxx::pool> pool_;
    // 操作db是放在线程池中进行的，这里记录下，防止多个连接同时去更新数据，保障更新顺序
    // 由于插入和删除都在调度线程中进行，因此这里不需要加锁
    // 只是对于每个info的数据，在读写的时候进行加锁
    using user_infos = std::unordered_set<user_info, std::hash<user_info>, std::equal_to<>>;
    user_infos user_infos_;
    using account_infos = std::unordered_set<account_info, std::hash<account_info>, std::equal_to<>>;
    account_infos account_infos_;
    using fn_message = std::function<simple::task<>(uint16_t, uint64_t, const simple::memory_buffer&)>;
    std::unordered_map<uint16_t, fn_message> message_callbacks_;
};
