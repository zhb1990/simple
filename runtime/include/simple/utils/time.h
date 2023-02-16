#pragma once
#include <simple/config.h>

#include <chrono>

namespace simple {

DS_API int32_t utc_minutes_offset(const std::chrono::system_clock::time_point&, const std::tm& lc);

inline int64_t get_timestamp_seconds() {
    using namespace std::chrono;
    return duration_cast<seconds>(high_resolution_clock::now().time_since_epoch()).count();
}

inline int64_t get_timestamp_millis() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(high_resolution_clock::now().time_since_epoch()).count();
}

inline int64_t get_system_clock_seconds() {
    using namespace std::chrono;
    return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

inline int64_t get_system_clock_millis() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

template <typename ToDuration>
ToDuration time_fraction(const std::chrono::system_clock::time_point& tp) {
    using std::chrono::duration_cast;
    using std::chrono::seconds;
    const auto duration = tp.time_since_epoch();
    const auto secs = duration_cast<seconds>(duration);
    return duration_cast<ToDuration>(duration) - duration_cast<ToDuration>(secs);
}

}  // namespace simple
