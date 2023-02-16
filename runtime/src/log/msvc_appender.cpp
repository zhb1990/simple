#if defined(_WIN32)

#include <Windows.h>
#include <simple/log/msvc_appender.h>

namespace simple {

void msvc_appender::write(log_level, const log_clock_point&, const log_buf_t& buf, size_t, size_t) {
    const auto len = MultiByteToWideChar(CP_UTF8, 0, buf.data(), static_cast<int>(buf.size()), nullptr, 0);
    if (len <= 0) return;
    std::wstring temp(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, buf.data(), static_cast<int>(buf.size()), temp.data(), static_cast<int>(temp.size()));
    OutputDebugStringW(temp.data());
}

void msvc_appender::flush_unlock() {}

}  // namespace simple

#endif
