#include <simple/log/console_appender.h>
#include <simple/log/file_appender.h>
#include <simple/log/log.h>
#include <simple/log/log_system.h>
#include <simple/log/msvc_appender.h>
#include <simple/utils/os.h>

#include <filesystem>
#include <simple/utils/toml_types.hpp>

namespace simple {

static bool read_pattern_and_time_type(std::string& pattern, log_time_type& time_type, const toml_table_t& table) {
    const auto it = table.find("pattern");
    if (it == table.end()) {
        return false;
    }

    if (!it->second.is_array()) {
        return false;
    }

    const auto& arr = it->second.as_array();
    if (arr.empty()) {
        return false;
    }

    const auto* temp = arr.data();
    if (!temp->is_string()) {
        return false;
    }

    pattern = temp->as_string();
    if (pattern.empty()) {
        return false;
    }

    if (arr.size() > 1 && temp[1].is_string()) {
        time_type = log_time_from_string_view(std::string_view(temp[1].as_string()));
    }

    return true;
}

static bool read_table_value(std::string& value, const std::string& key, const toml_table_t& table) {
    const auto it = table.find(key);
    if (it == table.end() || !it->second.is_string()) {
        return false;
    }

    value = it->second.as_string();
    return true;
}

static bool read_table_value(bool& value, const std::string& key, const toml_table_t& table) {
    const auto it = table.find(key);
    if (it == table.end() || !it->second.is_boolean()) {
        return false;
    }

    value = it->second.as_boolean();
    return true;
}

static bool read_table_value(toml::integer& value, const std::string& key, const toml_table_t& table) {
    const auto it = table.find(key);
    if (it == table.end() || !it->second.is_integer()) {
        return false;
    }

    value = it->second.as_integer();
    return true;
}

struct log_config_simple {
    std::string pattern;
    log_time_type time_type{log_time_type::local};
    log_level level{log_level::trace};
};

static bool read_file_appender_config(file_appender_config& config, const std::string& logger_name, const toml_table_t& table) {
    read_table_value(config.name, "name", table);
    if (config.name.empty()) {
        static std::atomic_uint16_t atomic_id{0};
        try {
            std::error_code ec;
            const auto program_name = os::program_location(ec).filename().replace_extension("");
            if (program_name.empty()) {
                if (const auto id = atomic_id.fetch_add(1, std::memory_order::relaxed); id == 0) {
                    config.name = logger_name;
                } else {
                    config.name = fmt::format("{}_{}", logger_name, id);
                }
            } else if (const auto id = atomic_id.fetch_add(1, std::memory_order::relaxed); id == 0) {
                config.name = fmt::format("{}_{}", reinterpret_cast<const char*>(program_name.u8string().c_str()), logger_name);
            } else {
                config.name =
                    fmt::format("{}_{}_{}", reinterpret_cast<const char*>(program_name.u8string().c_str()), logger_name, id);
            }
        } catch (...) {
            return false;
        }
    }

    read_table_value(config.log_directory, "log_directory", table);
    if (config.log_directory.empty()) {
        config.log_directory = "./";
    }

    read_table_value(config.lz4_directory, "lz4_directory", table);
    if (config.lz4_directory.empty()) {
        config.lz4_directory = "./";
    }

    toml::integer max_size{0};
    read_table_value(max_size, "max_size", table);
    if (max_size > 0) {
        config.max_size = max_size;
    }

    std::string temp;
    read_table_value(temp, "file_time", table);
    if (!temp.empty()) {
        config.file_time = log_time_from_string_view(temp);
    }

    read_table_value(config.daily_roll, "daily_roll", table);

    return true;
}

static log_appender_ptr create_appender_custom(const log_config_simple& logger_config, const std::string& logger_name,
                                               const toml_table_t& table) {
    log_config_simple custom_config = logger_config;
    read_pattern_and_time_type(custom_config.pattern, custom_config.time_type, table);
    std::string temp;
    read_table_value(temp, "level", table);
    if (!temp.empty()) {
        custom_config.level = log_level_from_string_view(temp);
    }

    if (!read_table_value(temp, "type", table)) {
        return {};
    }

    log_appender_ptr appender;
    if (temp == "stdout") {
        appender = std::make_shared<stdout_appender>();
    } else if (temp == "stderr") {
        appender = std::make_shared<stderr_appender>();
    } else if (temp == "file") {
        file_appender_config file_config;
        if (!read_file_appender_config(file_config, logger_name, table)) {
            return {};
        }
        appender = std::make_shared<file_appender>(file_config);
#if defined(_WIN32)
    } else if (temp == "msvc") {
        appender = std::make_shared<msvc_appender>();
#endif
    } else {
        return {};
    }

    appender->set_pattern(custom_config.pattern, custom_config.time_type);
    appender->set_level(custom_config.level);

    return appender;
}

static std::vector<log_appender_ptr> create_appenders_custom(const log_config_simple& logger_config,
                                                             const std::string& logger_name, const toml_table_t& table) {
    const auto it = table.find("appenders");
    if (it == table.end()) {
        return {};
    }

    if (!it->second.is_array()) {
        return {};
    }

    const auto& arr = it->second.as_array();
    if (arr.empty()) {
        return {};
    }

    std::vector<log_appender_ptr> appenders;
    for (const auto& val : arr) {
        if (!val.is_table()) {
            continue;
        }

        if (auto appender = create_appender_custom(logger_config, logger_name, val.as_table())) {
            appenders.emplace_back(std::move(appender));
        }
    }

    return appenders;
}

static logger_ptr create_logger_custom(const log_config_simple& global, const std::string& name, const toml_table_t& table) {
    log_config_simple custom_config = global;
    read_pattern_and_time_type(custom_config.pattern, custom_config.time_type, table);
    std::string temp;
    read_table_value(temp, "level", table);
    if (!temp.empty()) {
        custom_config.level = log_level_from_string_view(temp);
    }

    bool async = false;
    read_table_value(async, "async", table);

    const auto appenders = create_appenders_custom(custom_config, name, table);
    if (appenders.empty()) {
        return {};
    }

    auto ptr = std::make_shared<logger>(name, appenders, async);
    ptr->set_level(custom_config.level);

    return ptr;
}

void load_log_config(const std::string& utf8_path) {
    try {
        const std::filesystem::path path(reinterpret_cast<const char8_t*>(utf8_path.c_str()));
        std::ifstream ifs(path, std::ios_base::binary | std::ios_base::in);
        if (!ifs.is_open()) {
            return print_error("load_log_config open config {} fail,{}\n", utf8_path,
                               ERROR_CODE_MESSAGE(std::generic_category().message(errno)));
        }

        auto config = toml::parse(ifs, utf8_path);
        auto& root = config.as_table();

        auto& system = log_system::instance();
        system.drop_all();

        // 读取 全局 配置
        log_config_simple global;
        read_pattern_and_time_type(global.pattern, global.time_type, root);
        if (!global.pattern.empty()) {
            system.set_pattern(global.pattern, global.time_type);
        }
        std::string temp;
        read_table_value(temp, "level", root);
        global.level = log_level_from_string_view(temp);
        system.set_levels({}, &global.level);

        std::string default_log;
        read_table_value(default_log, "default", root);

        // 读取自定义logger配置
        for (const auto& [key, value] : root) {
            if (!value.is_table()) {
                continue;
            }

            auto custom = create_logger_custom(global, key, value.as_table());
            if (!custom) {
                continue;
            }

            if (key == default_log) {
                system.set_default(custom);
            } else {
                system.register_logger(custom);
            }
        }

        if (!system.default_logger()) {
            auto default_logger = std::make_shared<logger>("", std::make_shared<stdout_appender>(), false);
            if (!global.pattern.empty()) {
                default_logger->set_pattern(global.pattern, global.time_type);
            }
            default_logger->set_level(global.level);
            system.set_default(std::move(default_logger));
        }
    } catch (const std::exception& exception) {
        print_error("load_log_config {} fail,\n{}\n", utf8_path, exception.what());
    }
}

void register_logger(logger_ptr new_logger) { return log_system::instance().register_logger(std::move(new_logger)); }

void initialize_logger(logger_ptr new_logger) { return log_system::instance().initialize_logger(std::move(new_logger)); }

logger_ptr find_logger(const std::string& name) { return log_system::instance().find(name); }

void drop_logger(const std::string& name) { return log_system::instance().drop(name); }

void drop_all_loggers() { return log_system::instance().drop_all(); }

logger_ptr default_logger() { return log_system::instance().default_logger(); }

void set_default_logger(logger_ptr new_default_logger) {
    return log_system::instance().set_default(std::move(new_default_logger));
}

void set_log_level(log_level level) { return log_system::instance().set_level(level); }

void set_log_levels(log_levels levels, const log_level* global_level) {
    return log_system::instance().set_levels(std::move(levels), global_level);
}

void set_log_formatter(std::unique_ptr<log_formatter> formatter) {
    return log_system::instance().set_formatter(std::move(formatter));
}

void set_log_pattern(const std::string_view& pattern, log_time_type time_type) {
    return log_system::instance().set_pattern(pattern, time_type);
}

void log_flush() { return log_system::instance().flush(); }

void set_log_flush_interval(int64_t seconds) { return log_system::instance().set_flush_interval(seconds); }

}  // namespace simple
