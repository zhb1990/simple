#pragma once

#include <simple/config.h>

#include <cstdio>
#include <filesystem>
#include <string>
#include <string_view>

namespace simple::os {

DS_API int pid();

DS_API int tid();

bool is_color_terminal() noexcept;

bool in_terminal(FILE* file);

#if defined(_WIN32)

// 启用虚拟终端
DS_API bool open_virtual_terminal();

// 设置控制台的codepage 为 utf-8
DS_API void set_control_uft8();

DS_API std::string gbk_to_utf8(const std::string_view& gbk);

DS_API std::wstring utf8_to_wstring(const std::string_view& utf8);

DS_API void* set_crash_report(const std::string& app_name, const std::string& utf8_path);

DS_API void unset_crash_report(void* filter);

#endif

namespace fs = std::filesystem;

DS_API fs::path program_location(std::error_code& ec);

}  // namespace simple::os

#if defined(OS_ENABLE_UTF8)
#define ERROR_CODE_MESSAGE(msg) msg
#else
#define ERROR_CODE_MESSAGE(msg) simple::os::gbk_to_utf8(msg)
#endif
