#include <lz4frame.h>
#include <simple/log/console_appender.h>
#include <simple/log/log.h>
#include <simple/log/log_system.h>
#include <simple/log/logger.h>
#include <simple/utils/os.h>
#include <simple/utils/time.h>

#include <filesystem>
#include <fstream>
#include <vector>

namespace simple {

struct log_system::lz4_data {  // NOLINT(cppcoreguidelines-special-member-functions)
    lz4_data();

    ~lz4_data() noexcept;

    void compress(const std::string& src, const std::string& dest);

    bool compress_file(std::ofstream& output, uint64_t& count_out, uint64_t& count_in, std::ifstream& input);

    std::vector<char> input_chunk;
    std::vector<char> output_buff;
    LZ4F_compressionContext_t ctx{nullptr};
};

constexpr int64_t flush_interval_default = 5;

log_system::log_system() : pool_(&log_message::node), flush_interval_(flush_interval_default) {
#if defined(_WIN32)
    // windows启用虚拟终端，开启与linux一样的打印带颜色的log
    os::open_virtual_terminal();
#if !defined(OS_ENABLE_UTF8)
    os::set_control_uft8();
#endif
#endif

    lz4_data_ = std::make_unique<lz4_data>();
    default_logger_ = std::make_shared<logger>("", std::make_shared<stdout_appender>(), false);
    loggers_.emplace("", default_logger_);

    lz4_thread_ = std::jthread{[this](const std::stop_token& token) { return lz4_run(token); }};

    writer_thread_ = std::jthread{[this](const std::stop_token& token) { return writer_run(token); }};
}

log_system::~log_system() noexcept {
    writer_thread_.request_stop();
    {
        std::unique_lock lock(writer_mutex_);
        writer_cv_.notify_one();
    }
    lz4_thread_.request_stop();

    writer_thread_.join();
    lz4_thread_.join();

    // 关闭时候刷新下log
    backend_flush();

    while (true) {
        auto* n = writer_queue_.pop();
        if (!n) break;
        release_message(SIMPLE_CONVERT(n, log_message, node));
    }
}

log_system& log_system::instance() {
    static log_system system;
    return system;
}

void log_system::register_logger(logger_ptr new_logger) {
    std::lock_guard lock(loggers_mutex_);
    insert_loggers(std::move(new_logger));
}

void log_system::initialize_logger(logger_ptr new_logger) {
    std::lock_guard lock(loggers_mutex_);
    new_logger->set_formatter(formatter_->clone());

    if (const auto it = log_levels_.find(new_logger->name()); it != log_levels_.end()) {
        new_logger->set_level(it->second);
    } else {
        new_logger->set_level(global_level_);
    }

    insert_loggers(std::move(new_logger));
}

logger_ptr log_system::find(const std::string& name) {
    std::lock_guard lock(loggers_mutex_);
    if (const auto it = loggers_.find(name); it != loggers_.end()) {
        return it->second;
    }
    return {};
}

void log_system::drop(const std::string& name) {
    {
        std::lock_guard lock(loggers_mutex_);
        loggers_.erase(name);
    }

    const auto current = default_logger_.load(std::memory_order::relaxed);
    if (current && current->name() == name) {
        default_logger_.store({}, std::memory_order::relaxed);
    }
}

void log_system::drop_all() {
    {
        std::lock_guard lock(loggers_mutex_);
        loggers_.clear();
    }
    default_logger_.store({}, std::memory_order::relaxed);
}

logger_ptr log_system::default_logger() const { return default_logger_.load(std::memory_order::relaxed); }

void log_system::set_default(logger_ptr new_default_logger) {
    const auto old = default_logger_.load(std::memory_order::relaxed);
    {
        std::lock_guard lock(loggers_mutex_);
        if (old) {
            loggers_.erase(old->name());
        }
        if (new_default_logger) {
            insert_loggers(new_default_logger);
        }
    }

    return default_logger_.store(std::move(new_default_logger), std::memory_order::relaxed);
}

void log_system::set_level(log_level level) {
    std::lock_guard lock(loggers_mutex_);
    global_level_ = level;
    for (const auto& val : loggers_ | std::views::values) {
        val->set_level(level);
    }
}

void log_system::set_levels(log_levels levels, const log_level* global_level) {
    std::lock_guard lock(loggers_mutex_);

    log_levels_ = std::move(levels);
    if (global_level) {
        global_level_ = *global_level;
    }

    for (const auto& [name, ptr] : loggers_) {
        if (const auto it = log_levels_.find(name); it != log_levels_.end()) {
            ptr->set_level(it->second);
        } else if (global_level) {
            ptr->set_level(global_level_);
        }
    }
}

void log_system::set_formatter(std::unique_ptr<log_formatter> formatter) {
    std::lock_guard lock(loggers_mutex_);
    formatter_ = std::move(formatter);
    for (const auto& val : loggers_ | std::views::values) {
        val->set_formatter(formatter_->clone());
    }
}

void log_system::set_pattern(const std::string_view& pattern, log_time_type time_type) {
    auto new_formatter = std::make_unique<log_formatter>(pattern, time_type);
    set_formatter(std::move(new_formatter));
}

void log_system::flush() {
    std::lock_guard lock(loggers_mutex_);
    for (const auto& val : loggers_ | std::views::values) {
        val->flush();
    }
}

log_message* log_system::new_message() { return pool_.create(); }

void log_system::release_message(log_message* msg) {
    msg->buf = log_buf_t{};
    if (msg->ptr) {
        msg->ptr.reset();
    }

    return pool_.release(msg);
}

void log_system::send(log_message* msg) {
    writer_queue_.push(&msg->node);
    // 由于log一般会比较频繁，且会有定时刷新，这里就不加锁了
    writer_cv_.notify_one();
}

void log_system::set_flush_interval(int64_t seconds) {
    if (seconds >= 0) {
        flush_interval_.store(seconds, std::memory_order::relaxed);
        {
            std::unique_lock lock(writer_mutex_);
            writer_cv_.notify_one();
        }
    }
}

void log_system::post_lz4(const std::string& src, const std::string& dest) {
    std::unique_lock lock(lz4_mutex_);
    lz4_queue_.emplace_back(src, dest);
    lz4_cv_.notify_one();
}

void log_system::insert_loggers(logger_ptr ptr) {
    const auto& name = ptr->name();
    if (const auto it = loggers_.find(name); it != loggers_.end()) {
        print_error("replace logger with name '{}'\n", name);
        it->second = std::move(ptr);
    } else {
        loggers_.emplace(name, std::move(ptr));
    }
}

log_message* log_system::recv(const interval_t& dur) {
    auto* n = writer_queue_.pop();
    if (!n) {
        if (dur.count() == 0) {
            std::unique_lock lock(writer_mutex_);
            writer_cv_.wait(lock);
        } else {
            std::unique_lock lock(writer_mutex_);
            writer_cv_.wait_for(lock, dur);
        }
        n = writer_queue_.pop();
        if (!n) {
            return nullptr;
        }
    }

    return SIMPLE_CONVERT(n, log_message, node);
}

void log_system::backend_flush() {
    std::lock_guard lock(loggers_mutex_);
    for (const auto& val : loggers_ | std::views::values) {
        val->backend_flush();
    }
}

void log_system::writer_run(const std::stop_token& token) {
    auto last_flush = get_timestamp_seconds();
    auto last_interval = flush_interval_.load(std::memory_order::relaxed);
    interval_t dur = std::chrono::seconds(last_interval);
    while (!token.stop_requested()) {
        if (auto* msg = recv(dur)) {
            if (msg->ptr) {
                if (msg->tp == log_message::log) {
                    msg->ptr->backend_log(*msg);
                } else {
                    // msg->tp == log_message::flush
                    msg->ptr->backend_flush();
                }
            }

            release_message(msg);
        }

        if (const auto interval = flush_interval_.load(std::memory_order::relaxed); interval > 0) {
            if (const auto current = get_timestamp_seconds(); current - last_flush >= interval) {
                last_flush = current;
                dur = std::chrono::seconds(interval);
                backend_flush();
            } else {
                dur = std::chrono::seconds(interval - current + last_flush);
            }
            last_interval = interval;
        } else if (last_interval > 0) {
            last_interval = 0;
            dur = std::chrono::seconds(0);
        }
    }
}

void log_system::lz4_run(const std::stop_token& token) {
    while (!token.stop_requested()) {
        std::string src, dest;
        {
            std::unique_lock lock(lz4_mutex_);
            if (!lz4_cv_.wait(lock, token, [this]() { return !lz4_queue_.empty(); })) {
                continue;
            }

            std::tie(src, dest) = std::move(lz4_queue_.front());
            lz4_queue_.pop_front();
        }

        lz4_data_->compress(src, dest);
    }
}

static constexpr LZ4F_preferences_t lz4_preferences = {
    {LZ4F_max256KB, LZ4F_blockLinked, LZ4F_noContentChecksum, LZ4F_frame, 0 /* unknown content size */, 0 /* no dictID */,
     LZ4F_noBlockChecksum},
    0,         /* compression level; 0 == default */
    0,         /* auto flush */
    0,         /* favor decompression speed */
    {0, 0, 0}, /* reserved, must be set to 0 */
};

constexpr size_t lz4_input_chunk_size = 16ull * 1024;

class lz4_exception final : public std::exception {
  public:
    explicit lz4_exception(size_t ec) : ec_(ec) {}
    [[nodiscard]] const char* what() const noexcept override { return LZ4F_getErrorName(ec_); }

  private:
    size_t ec_;
};

log_system::lz4_data::lz4_data() {
    input_chunk.resize(lz4_input_chunk_size);
    output_buff.resize(LZ4F_compressBound(lz4_input_chunk_size, &lz4_preferences));
    if (const size_t ec = LZ4F_createCompressionContext(&ctx, LZ4F_VERSION); LZ4F_isError(ec)) {
        throw lz4_exception(ec);
    }
}

log_system::lz4_data::~lz4_data() noexcept { LZ4F_freeCompressionContext(ctx); }

void log_system::lz4_data::compress(const std::string& src, const std::string& dest) {
    auto stamp = get_timestamp_millis();
    // 转成utf-8指针
    std::ifstream input;
    os::fs::path path_src = reinterpret_cast<const char8_t*>(src.c_str());
    input.open(path_src, std::ios_base::binary | std::ios_base::in);
    if (!input.is_open()) {
        print_error("compress open input {} fail, {}\n", src, std::generic_category().message(errno));
        return;
    }

    // 转成utf-8指针
    std::ofstream output;
    os::fs::path path_dest = reinterpret_cast<const char8_t*>(dest.c_str());
    output.open(path_dest, std::ios_base::binary | std::ios_base::out | std::ios_base::trunc);
    if (!output.is_open()) {
        print_error("compress open output {} fail, {}\n", src, std::generic_category().message(errno));
        return;
    }

    uint64_t count_out = 0;
    uint64_t count_in = 0;
    if (compress_file(output, count_out, count_in, input)) {
        if (count_in > 0) {
            stamp = get_timestamp_millis() - stamp;
            auto rate = static_cast<double>(count_out) / static_cast<double>(count_in);
            print_error("{}: compress {} -> {} bytes, {:.2}, {}ms\n", src, count_in, count_out, rate, stamp);
        }
        input.close();
        std::error_code ec;
        std::filesystem::remove(path_src, ec);
        if (ec) {
            print_error("after compress remove fail, {}\n", ERROR_CODE_MESSAGE(ec.message()));
        }
    }
}

bool log_system::lz4_data::compress_file(std::ofstream& output, uint64_t& count_out, uint64_t& count_in, std::ifstream& input) {
    /* write frame header */
    auto const header_size = LZ4F_compressBegin(ctx, output_buff.data(), output_buff.size(), &lz4_preferences);
    if (LZ4F_isError(header_size)) {
        print_error("Failed to start compression: error 0x{:x}\n", header_size);
        return false;
    }
    output.write(output_buff.data(), static_cast<std::streamsize>(header_size));
    count_out = header_size;

    /* stream file */
    while (!input.eof()) {
        input.read(input_chunk.data(), lz4_input_chunk_size);
        const auto read_size = static_cast<size_t>(input.gcount());
        /* nothing left to read from input file */
        if (read_size == 0) break;
        count_in += read_size;

        auto const compressed_size =
            LZ4F_compressUpdate(ctx, output_buff.data(), output_buff.size(), input_chunk.data(), read_size, nullptr);
        if (LZ4F_isError(compressed_size)) {
            print_error("Compression failed: error 0x{:x}\n", compressed_size);
            return false;
        }
        output.write(output_buff.data(), static_cast<std::streamsize>(compressed_size));
        count_out += compressed_size;
    }

    /* flush whatever remains within internal buffers */
    auto const compressed_size = LZ4F_compressEnd(ctx, output_buff.data(), output_buff.size(), nullptr);
    if (LZ4F_isError(compressed_size)) {
        print_error("Failed to end compression: error 0x{:x}\n", compressed_size);
        return false;
    }
    output.write(output_buff.data(), static_cast<std::streamsize>(compressed_size));
    count_out += compressed_size;

    return true;
}

}  // namespace simple
