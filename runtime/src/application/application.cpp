#include <simple/application/application.h>
#include <simple/coro/network.h>
#include <simple/coro/scheduler.h>
#include <simple/coro/thread_pool.h>
#include <simple/coro/timed_awaiter.h>
#include <simple/log/log.h>
#include <simple/net/socket_system.h>
#include <simple/shm/shm_channel_select.h>
#include <simple/utils/os.h>

#include <algorithm>
#include <filesystem>
#include <ranges>
#include <simple/application/service.hpp>
#include <simple/coro/co_start.hpp>
#include <simple/coro/parallel_task.hpp>
#include <simple/coro/sync_wait.hpp>
#include <stdexcept>

#include "service_loader.h"

namespace simple {

application::~application() noexcept = default;

application& application::instance() {
    static application ins;
    return ins;
}

void application::start(const std::string& utf8_path) {
    // 解析配置文件
    const std::filesystem::path path(reinterpret_cast<const char8_t*>(utf8_path.c_str()));
    std::ifstream ifs(path, std::ios_base::binary | std::ios_base::in);
    if (!ifs.is_open()) {
        return print_error("application open config {} fail,{}\n", utf8_path,
                           ERROR_CODE_MESSAGE(std::generic_category().message(errno)));
    }

    auto config = toml::parse(ifs, utf8_path);
    config_ = std::move(config.as_table());

    // 设置log配置文件
    if (const auto it = config_.find("log_config"); it != config_.end() && it->second.is_string()) {
        load_log_config(it->second.as_string());
    }

    // 设置帧间隔
    if (const auto it = config_.find("frame_interval"); it != config_.end() && it->second.is_integer()) {
        frame_interval_ = std::chrono::milliseconds(it->second.as_integer());
    } else {
        constexpr auto frame_interval_default = 500ll;
        frame_interval_ = std::chrono::milliseconds(frame_interval_default);
    }

#if defined(_WIN32)
    std::string app_name;
    if (const auto it = config_.find("name"); it != config_.end() && it->second.is_string()) {
        app_name = it->second.as_string();
    }

    std::string dump_path;
    if (const auto it = config_.find("dump_path"); it != config_.end() && it->second.is_string()) {
        dump_path = it->second.as_string();
    } else {
        dump_path = "./";
    }

    auto crash_filter = os::set_crash_report(app_name, dump_path);
#endif

    // 注册回调函数
    socket_system::instance().register_signal_callback([this](int sig) { stop(); });
    network::instance().init();

    // 启动逻辑线程
    scheduler::instance().start();
    // 启动网络线程
    socket_system::instance().start();
    // 启动检查共享内存通道的线程
    shm_channel_select::instance().start();
    // 启动线程池
    size_t thread_pool_num = 1;
    if (const auto it = config_.find("thread_pool_num"); it != config_.end() && it->second.is_integer()) {
        thread_pool_num = it->second.as_integer();
    }
    thread_pool::instance().start(thread_pool_num);

    // 设置服务的查找路径
    if (const auto it = config_.find("service_path"); it != config_.end() && it->second.is_array()) {
        for (const auto& services = it->second.as_array(); const auto& service : services) {
            service_loader::instance().add_path(service.as_string());
        }
    }

    // 加载服务
    load_services();

    // 启动服务
    awake_services();

    // 启动更新游戏帧的协程
    update_frame();

    // 等待所有线程退出
    scheduler::instance().join();
    socket_system::instance().join();
    thread_pool::instance().join();
    shm_channel_select::instance().join();

    release_services();

#if defined(_WIN32)
    os::unset_crash_report(crash_filter);
#endif
}

void application::stop() {
    shm_channel_select::instance().stop();
    socket_system::instance().stop();
    scheduler::instance().stop();
    thread_pool::instance().stop();
}

bool application::contains_service(uint16_t id) const { return service_map_.contains(id); }

void application::register_message_callback(uint32_t id, const service_base* service, message_callback callback) {
    auto& callbacks = message_callbacks_[id];
    callbacks.emplace(service, std::move(callback));
}

void application::deregister_message_callback(uint32_t id, const service_base* service) {
    const auto it_message = message_callbacks_.find(id);
    if (it_message == message_callbacks_.end()) {
        return;
    }

    const auto it_callback = it_message->second.find(service);
    if (it_callback == it_message->second.end()) {
        return;
    }

    it_message->second.erase(it_callback);
    if (it_message->second.empty()) {
        message_callbacks_.erase(it_message);
    }
}

task<> application::forward_message(uint32_t id, const memory_buffer& message) {
    std::vector<task<>> tasks;
    const auto it_message = message_callbacks_.find(id);
    if (it_message == message_callbacks_.end() || it_message->second.empty()) {
        co_return;
    }

    tasks.reserve(it_message->second.size());
    for (auto& callback : it_message->second | std::views::values) {
        tasks.emplace_back(callback(message));
    }

    for (const auto results = co_await when_ready(wait_type::all, tasks);
         const auto& result : results) {
        if (const auto e = result.get_exception()) {
            std::rethrow_exception(e);
        }
    }
}

void application::wait_frame(uint64_t frame, std::coroutine_handle<> handle) { frame_coroutine_[frame].emplace(handle); }

bool application::remove_frame_coroutine(uint64_t frame, std::coroutine_handle<> handle) {
    const auto it = frame_coroutine_.find(frame);
    if (it == frame_coroutine_.end()) {
        return false;
    }

    return it->second.erase(handle) > 0;
}

void application::load_services() {
    const auto it_services = config_.find("services");
    if (it_services == config_.end() || !it_services->second.is_array()) {
        return;
    }

    const auto& services = it_services->second.as_array();
    if (services.empty()) {
        return;
    }

    auto& loader = service_loader::instance();
    toml_value_t empty_value;
    for (const auto& value : services) {
        if (!value.is_table()) {
            continue;
        }

        auto& config = value.as_table();
        const auto it_name = config.find("name");
        if (it_name == config.end() || !it_name->second.is_string()) {
            continue;
        }

        const auto it_id = config.find("id");
        if (it_id == config.end() || !it_id->second.is_integer()) {
            continue;
        }

        const auto id = static_cast<uint16_t>(it_id->second.as_integer());
        if (service_map_.contains(id)) {
            throw std::logic_error(fmt::format("service {} repeated", id));
        }

        const auto it_type = config.find("type");
        if (it_type == config.end() || !it_type->second.is_string()) {
            continue;
        }

        std::string type = it_type->second.as_string();
        auto* dll = loader.query(type);
        service_base* service;
        if (const auto it = config.find("args"); it != config.end()) {
            service = dll->create(&it->second);
        } else {
            service = dll->create(&empty_value);
        }

        service->name_ = it_name->second.as_string();
        service->type_ = type;
        service->id_ = id;

        if (const auto it = config.find("order"); it != config.end() && it->second.is_integer()) {
            service->order_ = static_cast<uint32_t>(it->second.as_integer());
        }

        if (const auto it = config.find("interval"); it != config.end() && it->second.is_integer()) {
            service->interval_ = it->second.as_integer();
        }

        service_map_.emplace(id, service);
        service_sort_.emplace_back(service);
    }

    // 加载完后按优先级排下序
    std::ranges::sort(service_sort_, [](const service_base* a, const service_base* b) { return a->order() < b->order(); });
}

// ReSharper disable once CppMemberFunctionMayBeConst
void application::awake_services() {
    std::string fail_name;
    try {
        sync_wait([this, &fail_name]() -> task<> {
            std::vector<task<>> tasks;
            tasks.reserve(service_sort_.size());

            for (auto* service : service_sort_) {
                tasks.emplace_back(service->awake());
            }

            size_t index = 0;
            for (const auto results = co_await when_ready(wait_type::one_fail, tasks);
                 const auto& result : results) {
                if (const auto e = result.get_exception()) {
                    fail_name = service_sort_[index]->name();
                    std::rethrow_exception(e);
                }
                ++index;
            }
        }());
    } catch (const std::exception& e) {
        error("service {} awake fail {}", fail_name, ERROR_CODE_MESSAGE(e.what()));
        stop();
    } catch (...) {
        error("service {} awake fail", fail_name);
        stop();
    }
}

void application::release_services() {
    auto& loader = service_loader::instance();
    for (const auto* service : service_sort_) {
        auto* dll = loader.query(service->type());
        dll->release(service);
    }
    service_sort_.clear();
    service_map_.clear();
}

// ReSharper disable once CppMemberFunctionMayBeConst
void application::update_frame() {
    co_start([this]() -> task<> {
        std::string fail_name;
        try {
            std::vector<task<>> tasks;
            tasks.reserve(service_sort_.size());
            std::vector<service_base*> update_services;
            update_services.reserve(service_sort_.size());
            auto current_time = timer_queue::clock::now();
            for (;;) {
                for (auto* service : service_sort_) {
                    if (service->need_update()) {
                        tasks.emplace_back(service->update());
                        update_services.emplace_back(service);
                    }
                }

                size_t index = 0;
                for (const auto results = co_await when_ready(wait_type::all, tasks);
                     const auto& result : results) {
                    if (const auto e = result.get_exception()) {
                        fail_name = update_services[index]->name();
                        std::rethrow_exception(e);
                    }
                    ++index;
                }

                tasks.clear();
                update_services.clear();
                current_time += frame_interval_;
                co_await sleep_until(current_time);
                ++frame_;

                // 唤醒等待当前帧的协程
                wake_up_frame();
            }
        } catch (const std::exception& e) {
            error("service {} update fail {}", fail_name, ERROR_CODE_MESSAGE(e.what()));
            stop();
        } catch (...) {
            error("service {} update fail", fail_name);
            stop();
        }
    });
}

void application::wake_up_frame() {
    const auto it = frame_coroutine_.find(frame_);
    if (it == frame_coroutine_.end()) {
        return;
    }

    auto& scheduler = scheduler::instance();
    for (auto& handle : it->second) {
        scheduler.wake_up_coroutine(handle);
    }

    frame_coroutine_.erase(it);
}

}  // namespace simple
