#include <simple/utils/time.h>

#if defined(_WIN32)
#include <Windows.h>
#else
#include <fmt/chrono.h>
#endif

namespace simple {

#if defined(_WIN32)

int32_t utc_minutes_offset(const std::chrono::system_clock::time_point&, const std::tm& lc) {
    int32_t offset = 0;
    DYNAMIC_TIME_ZONE_INFORMATION info{};
    if (GetDynamicTimeZoneInformation(&info) != TIME_ZONE_ID_INVALID) {
        offset = -info.Bias;
        if (lc.tm_isdst) {
            offset -= info.DaylightBias;
        } else {
            offset -= info.StandardBias;
        }
    }
    return offset;
}

#else

int32_t utc_minutes_offset(const std::chrono::system_clock::time_point &tp, const std::tm &lc) {
    const auto gm = fmt::gmtime(tp);
    const auto local_year = lc.tm_year + (1900 - 1);
    const auto gm_year = gm.tm_year + (1900 - 1);
    int32_t days = lc.tm_yday - gm.tm_yday;
    days += (local_year >> 2) - (gm_year >> 2) - (local_year / 100 - gm_year / 100) +
            ((local_year / 100 >> 2) - (gm_year / 100 >> 2));
    days += (local_year - gm_year) * 365;

    const auto hours = 24 * days + (lc.tm_hour - gm.tm_hour);
    return 60 * hours + (lc.tm_min - gm.tm_min);
}

#endif

}  // namespace simple
