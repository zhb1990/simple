#pragma once
#include <simple/containers/queue.h>
#include <simple/log/types.h>

#include <source_location>

namespace simple {

inline constexpr size_t log_data_default_size = 256;

struct log_message {  // NOLINT(cppcoreguidelines-pro-type-member-init)
    const_logger_ptr ptr;

    enum message_type { log, flush };
    message_type tp{log};
    log_level level{log_level::off};
    log_clock_point point;
    int32_t tid;
    std::source_location source;
    log_buf_t buf;

    mutable size_t color_start{0};
    mutable size_t color_stop{0};

    mpsc_queue_base::node node;
};

}  // namespace simple
