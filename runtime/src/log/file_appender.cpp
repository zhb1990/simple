#include <fmt/chrono.h>
#include <simple/log/file_appender.h>
#include <simple/log/log.h>
#include <simple/log/log_system.h>
#include <simple/utils/os.h>

namespace simple {

file_appender::file_appender(file_appender_config config) : config_(std::move(config)) {
    cached_point_ = log_clock::now();
    if (config_.file_time == log_time_type::local) {
        cached_tm_ = fmt::localtime(cached_point_);
    } else {
        cached_tm_ = fmt::gmtime(cached_point_);
    }

    std::error_code ec;
    std::filesystem::path directory(reinterpret_cast<const char8_t*>(config_.log_directory.c_str()));
    create_directories(directory, ec);
    if (ec) {
        print_error("file_appender create directory {} fail, {}\n", config_.log_directory, ERROR_CODE_MESSAGE(ec.message()));
    }

    directory = reinterpret_cast<const char8_t*>(config_.lz4_directory.c_str());
    create_directories(directory, ec);
    if (ec) {
        print_error("file_appender create directory {} fail, {}\n", config_.lz4_directory, ERROR_CODE_MESSAGE(ec.message()));
    }
}

void file_appender::write(log_level, const log_clock_point& point, const log_buf_t& buf, size_t, size_t) {
    using std::chrono::seconds;

    if (duration_cast<seconds>(point.time_since_epoch()) > duration_cast<seconds>(cached_point_.time_since_epoch())) {
        std::tm current_tm;  // NOLINT(cppcoreguidelines-pro-type-member-init)
        if (config_.file_time == log_time_type::local) {
            current_tm = fmt::localtime(point);
        } else {
            current_tm = fmt::gmtime(point);
        }

        if (config_.daily_roll && (current_tm.tm_year != cached_tm_.tm_year || current_tm.tm_yday != cached_tm_.tm_yday)) {
            roll_file();
        }

        cached_tm_ = current_tm;
        cached_point_ = point;
    }

    if (!file_stream_.is_open()) {
        make_full_name();
        file_stream_.open(full_name_, std::ios_base::binary | std::ios_base::out | std::ios_base::trunc);
        if (!file_stream_.is_open()) {
            print_error("file_appender::open {} fail, {}\n", reinterpret_cast<const char*>(full_name_.u8string().c_str()),
                        std::generic_category().message(errno));
            return;
        }
    }

    file_stream_.write(buf.data(), static_cast<std::streamsize>(buf.size()));
    file_size_ += buf.size();

    if (config_.max_size > 0 && file_size_ >= config_.max_size) {
        roll_file();
    }
}

void file_appender::flush_unlock() {
    if (file_stream_.is_open()) {
        file_stream_.flush();
    }
}

void file_appender::make_full_name() {
    using std::chrono::seconds;
    if (const auto current = duration_cast<seconds>(cached_point_.time_since_epoch()); file_secs_ < current) {
        file_secs_ = current;
        file_id_ = 0;
    }

    std::string name;
    try {
        if (file_id_ == 0) {
            fmt::format_to(std::back_inserter(name), "{}_{:%Y-%m-%d_%H-%M-%S}_{}.log", config_.name, cached_tm_, os::pid());
        } else {
            fmt::format_to(std::back_inserter(name), "{}_{:%Y-%m-%d_%H-%M-%S}_{}_{}.log", config_.name, cached_tm_, os::pid(),
                           file_id_);
        }
    } catch (...) {
    }

    const std::filesystem::path file_name(reinterpret_cast<const char8_t*>(name.c_str()));
    full_name_ = reinterpret_cast<const char8_t*>(config_.log_directory.c_str());
    full_name_ /= file_name;
    ++file_id_;
}

void file_appender::roll_file() {
    if (!file_stream_.is_open()) {
        return;
    }

    file_stream_.close();
    file_size_ = 0;

    std::filesystem::path path(reinterpret_cast<const char8_t*>(config_.lz4_directory.c_str()));
    path /= full_name_.filename();
    path.replace_extension(".log.lz4");

    log_system::instance().post_lz4(reinterpret_cast<const char*>(full_name_.u8string().c_str()),
                                    reinterpret_cast<const char*>(path.u8string().c_str()));
}

}  // namespace simple
