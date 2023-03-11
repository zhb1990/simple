#pragma once
#include <simple/log/logmsg.h>

#include <condition_variable>
#include <deque>
#include <simple/containers/pool.hpp>
#include <thread>

namespace simple {

class log_system {
  protected:
    log_system();

  public:
    SIMPLE_NON_COPYABLE(log_system)

    SIMPLE_API ~log_system() noexcept;

    SIMPLE_API static log_system& instance();

    SIMPLE_API void register_logger(logger_ptr new_logger);

    SIMPLE_API void initialize_logger(logger_ptr new_logger);

    SIMPLE_API logger_ptr find(const std::string& name);

    SIMPLE_API void drop(const std::string& name);

    SIMPLE_API void drop_all();

    SIMPLE_API logger_ptr default_logger() const;

    SIMPLE_API void set_default(logger_ptr new_default_logger);

    SIMPLE_API void set_level(log_level level);

    SIMPLE_API void set_levels(log_levels levels, const log_level* global_level = nullptr);

    SIMPLE_API void set_formatter(std::unique_ptr<log_formatter> formatter);

    SIMPLE_API void set_pattern(const std::string_view& pattern, log_time_type time_type = log_time_type::local);

    SIMPLE_API void flush();

    SIMPLE_API log_message* new_message();

    SIMPLE_API void release_message(log_message* msg);

    SIMPLE_API void send(log_message* msg);

    SIMPLE_API void set_flush_interval(int64_t seconds);

    SIMPLE_API void post_lz4(const std::string& src, const std::string& dest);

  private:
    void insert_loggers(logger_ptr ptr);

    using interval_t = std::chrono::high_resolution_clock::duration;

    log_message* recv(const interval_t& dur);

    void backend_flush();

    void writer_run(const std::stop_token& token);

    void lz4_run(const std::stop_token& token);

    pool<log_message, 1024> pool_;

    std::mutex loggers_mutex_;
    std::unordered_map<std::string, logger_ptr> loggers_;
    log_levels log_levels_;
    std::unique_ptr<log_formatter> formatter_;
    log_level global_level_{log_level::trace};
    std::atomic<logger_ptr> default_logger_;

    // 写日志的线程
    std::jthread writer_thread_;
    // 仅一个log线程，选择单消费者队列
    mpsc_queue writer_queue_;
    std::mutex writer_mutex_;
    std::condition_variable writer_cv_;
    std::atomic_int64_t flush_interval_{0};

    // 压缩日志线程
    std::jthread lz4_thread_;
    std::deque<std::pair<std::string, std::string>> lz4_queue_;
    std::mutex lz4_mutex_;
    std::condition_variable_any lz4_cv_;
    struct lz4_data;
    std::unique_ptr<lz4_data> lz4_data_;
};

}  // namespace simple
