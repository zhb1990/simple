#pragma once
#include <simple/config.h>

#include <coroutine>
#include <functional>
#include <simple/containers/buffer.hpp>
#include <simple/coro/task.hpp>
#include <simple/utils/toml_types.hpp>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace simple {

class service_base;

class application {
    application() = default;

  public:
    SIMPLE_NON_COPYABLE(application)

    SIMPLE_API ~application() noexcept;

    SIMPLE_API static application& instance();

    /**
     * \brief 根据配置文件启动
     * \param utf8_path 配置文件的utf8路径
     */
    SIMPLE_API void start(const std::string& utf8_path);

    /**
     * \brief 关闭
     */
    SIMPLE_API static void stop();

    /**
     * \brief 查看是否有加载对应名字的服务
     * \param id 服务id
     * \return 返回 是否有该服务
     */
    [[nodiscard]] SIMPLE_API bool contains_service(uint16_t id) const;

    using message_callback = std::function<task<>(const memory_buffer&)>;

    /**
     * \brief 注册消息的回调函数
     * \param id 消息id
     * \param service 服务类指针
     * \param callback 回调函数
     */
    SIMPLE_API void register_message_callback(uint32_t id, const service_base* service, message_callback callback);

    /**
     * \brief 取消注册消息的回调函数
     * \param id 消息id
     * \param service 服务类指针
     */
    SIMPLE_API void deregister_message_callback(uint32_t id, const service_base* service);

    /**
     * \brief 分发消息
     * \param id 消息id
     * \param message 消息内容
     * \return 无返回的task
     */
    SIMPLE_API task<> forward_message(uint32_t id, const memory_buffer& message);

    /**
     * \brief 获取当前帧
     * \return 当前帧
     */
    [[nodiscard]] uint64_t frame() const noexcept { return frame_; }

    /**
     * \brief 获取配置文件
     * \return 配置文件的引用
     */
    [[nodiscard]] const toml_table_t& config() const noexcept { return config_; }

    SIMPLE_API void wait_frame(uint64_t frame, std::coroutine_handle<> handle);

    SIMPLE_API bool remove_frame_coroutine(uint64_t frame, std::coroutine_handle<> handle);

  private:
    void load_services();

    void awake_services();

    void release_services();

    void update_frame();

    void wake_up_frame();

    std::unordered_map<uint16_t, service_base*> service_map_;
    std::vector<service_base*> service_sort_;
    std::chrono::milliseconds frame_interval_{0};
    uint64_t frame_{0};
    using wait_coroutine_set = std::unordered_set<std::coroutine_handle<>>;
    // 每帧上的等待协程
    std::unordered_map<uint64_t, wait_coroutine_set> frame_coroutine_;
    toml_table_t config_;

    // 消息分发
    std::unordered_map<uint32_t, std::unordered_map<const service_base*, message_callback>> message_callbacks_;
};

}  // namespace simple
