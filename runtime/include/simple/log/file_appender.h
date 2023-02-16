#pragma once
#include <simple/log/appender.h>

#include <filesystem>
#include <fstream>

namespace simple {

struct file_appender_config {
    // log文件的名字（不含目录和扩展名）
    std::string name;
    // log文件的目录
    std::string log_directory;
    // log文件压缩的目录
    std::string lz4_directory;
    // 单个log文件最大长度
    std::size_t max_size{0};
    // 按本地时间还是utc时间
    log_time_type file_time{log_time_type::local};
    // 是否每天刷新log文件
    bool daily_roll{false};
};

class file_appender final : public log_appender {
  public:
    SIMPLE_API explicit file_appender(file_appender_config config);

    ~file_appender() noexcept override = default;

    SIMPLE_NON_COPYABLE(file_appender)

  protected:
    SIMPLE_API void write(log_level, const log_clock_point& point, const log_buf_t& buf, size_t, size_t) override;

    SIMPLE_API void flush_unlock() override;

  private:
    void make_full_name();

    void roll_file();

    std::filesystem::path full_name_;
    std::chrono::seconds file_secs_{0};
    size_t file_id_{0};
    size_t file_size_{0};
    int64_t last_open_{0};
    log_clock_point cached_point_;
    std::tm cached_tm_{};
    std::ofstream file_stream_;
    file_appender_config config_;
};

}  // namespace simple
