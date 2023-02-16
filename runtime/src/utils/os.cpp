#include <simple/utils/os.h>

#if defined(_WIN32)
#include <Windows.h>
#include <io.h>
#include <minidumpapiset.h>
#include <process.h>
#elif defined(macintosh) || defined(Macintosh) || (defined(__APPLE__) && defined(__MACH__))  // mac ios
#include <mach-o/dyld.h>
#include <pthread.h>
#include <unistd.h>
#else
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include <fmt/chrono.h>
#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <tuple>

namespace simple::os {

#if defined(_WIN32)

int pid() {
    static const auto v = _getpid();
    return v;
}

// Determine if the terminal supports colors
// Based on: https://github.com/agauniyal/rang/
bool is_color_terminal() noexcept { return true; }

// Determine if the terminal attached
// Source: https://github.com/agauniyal/rang/
bool in_terminal(FILE* file) { return _isatty(_fileno(file)) != 0; }

#else

int pid() {
    static const auto v = ::getpid();
    return v;
}

// Determine if the terminal supports colors
// Based on: https://github.com/agauniyal/rang/
bool is_color_terminal() noexcept {
    static const bool result = []() {
        const char *env_colorterm_p = std::getenv("COLORTERM");
        if (env_colorterm_p != nullptr) {
            return true;
        }

        static constexpr std::array<const char *, 16> terms = {{"ansi", "color", "console", "cygwin", "gnome", "konsole",
                                                                "kterm", "linux", "msys", "putty", "rxvt", "screen", "vt100",
                                                                "xterm", "alacritty", "vt102"}};
        const char *env_term_p = std::getenv("TERM");
        if (env_term_p == nullptr) {
            return false;
        }

        return std::any_of(terms.begin(), terms.end(),
                           [&](const char *term) { return std::strstr(env_term_p, term) != nullptr; });
    }();

    return result;
}

// Determine if the terminal attached
// Source: https://github.com/agauniyal/rang/
bool in_terminal(FILE *file) { return ::isatty(fileno(file)) != 0; }

#endif

#if defined(_WIN32)

int tid() {
    static thread_local const auto v = GetCurrentThreadId();
    return static_cast<int>(v);
}

bool open_virtual_terminal() {
    // 获取控制台的控制句柄
    const HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (handle == INVALID_HANDLE_VALUE) {
        return false;
    }
    // 获取控制台模式
    DWORD mode = 0;
    if (!GetConsoleMode(handle, &mode)) {
        return false;
    }
    // 增加控制台模式的选项：启用虚拟终端
    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (!SetConsoleMode(handle, mode)) {
        return false;
    }
    return true;
}

void set_control_uft8() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
}

std::string gbk_to_utf8(const std::string_view& gbk) {
    auto len = MultiByteToWideChar(CP_ACP, 0, gbk.data(), static_cast<int>(gbk.size()), nullptr, 0);
    if (len <= 0) return {};
    std::wstring temp(len, 0);
    MultiByteToWideChar(CP_ACP, 0, gbk.data(), static_cast<int>(gbk.size()), temp.data(), static_cast<int>(temp.size()));
    len = WideCharToMultiByte(CP_UTF8, 0, temp.data(), static_cast<int>(temp.size()), nullptr, 0, nullptr, nullptr);
    if (len <= 0) return {};
    std::string result(len, 0);
    WideCharToMultiByte(CP_UTF8, 0, temp.data(), static_cast<int>(temp.size()), result.data(), static_cast<int>(result.size()),
                        nullptr, nullptr);
    return result;
}

std::wstring utf8_to_wstring(const std::string_view& utf8) {
    const auto len = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (len <= 0) return {};

    std::wstring temp(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), temp.data(), static_cast<int>(temp.size()));
    return temp;
}

static std::string s_dump_name;
static std::filesystem::path s_dump_path;

static std::filesystem::path& get_program_name() {
    std::error_code ec;
    static std::filesystem::path name = program_location(ec).filename().replace_extension("");
    return name;
}

BOOL CALLBACK mini_dump_callback(PVOID, const PMINIDUMP_CALLBACK_INPUT input, PMINIDUMP_CALLBACK_OUTPUT output) {
    // Check parameters
    if (!input) return FALSE;
    if (!output) return FALSE;

    BOOL ret = FALSE;
    switch (input->CallbackType) {
        case IncludeModuleCallback:  // Include the module into the dump
        case IncludeThreadCallback:  // Include the thread into the dump
        case ThreadCallback:         // Include all thread information into the mini dump
        case ThreadExCallback:       // Include this information
        case ModuleCallback:
            ret = TRUE;
            break;
        default:
            break;
    }

    return ret;
}

static LONG generate_mini_dump(PEXCEPTION_POINTERS exception_pointers) {
    using mini_dump_write_dump_t =
        std::add_pointer_t<BOOL(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE, PMINIDUMP_EXCEPTION_INFORMATION,
                                PMINIDUMP_USER_STREAM_INFORMATION, PMINIDUMP_CALLBACK_INFORMATION)>;

    mini_dump_write_dump_t mini_dump_write_dump = nullptr;
    auto* dbg_help = LoadLibraryA("DbgHelp.dll");
    if (dbg_help) {
        mini_dump_write_dump =
            reinterpret_cast<mini_dump_write_dump_t>(reinterpret_cast<void*>(GetProcAddress(dbg_help, "MiniDumpWriteDump")));
    }

    if (mini_dump_write_dump) {
        const auto file_name = fmt::format("{}_{:%Y-%m-%d_%H-%M-%S}_{}.dmp", s_dump_name, fmt::localtime(time(nullptr)), pid());
        auto full_path = s_dump_path;
        full_path /= reinterpret_cast<const char8_t*>(file_name.c_str());

        auto* dump_file = CreateFileW(full_path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ,
                                      nullptr, CREATE_ALWAYS, 0, nullptr);
        if (dump_file != INVALID_HANDLE_VALUE) {
            MINIDUMP_EXCEPTION_INFORMATION exception_param;
            exception_param.ThreadId = GetCurrentThreadId();
            exception_param.ExceptionPointers = exception_pointers;
            exception_param.ClientPointers = FALSE;

            MINIDUMP_CALLBACK_INFORMATION mci;
            mci.CallbackRoutine = static_cast<MINIDUMP_CALLBACK_ROUTINE>(mini_dump_callback);
            mci.CallbackParam = nullptr;

            constexpr auto dump_type =
                static_cast<MINIDUMP_TYPE>(MiniDumpWithPrivateReadWriteMemory | MiniDumpWithDataSegs | MiniDumpWithHandleData |
                                           MiniDumpWithFullMemoryInfo | MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules);
            mini_dump_write_dump(GetCurrentProcess(), GetCurrentProcessId(), dump_file, dump_type,
                                 (exception_pointers ? &exception_param : nullptr), nullptr, &mci);
            CloseHandle(dump_file);
        }
    }

    if (dbg_help) FreeLibrary(dbg_help);

    return EXCEPTION_EXECUTE_HANDLER;
}

void* set_crash_report(const std::string& app_name, const std::string& utf8_path) {
    std::error_code ec;
    s_dump_name = app_name;
    if (s_dump_name.empty()) {
        s_dump_name = reinterpret_cast<const char*>(get_program_name().u8string().c_str());
    }
    s_dump_path = reinterpret_cast<const char8_t*>(utf8_path.c_str());
    create_directory(s_dump_path, ec);

    return reinterpret_cast<void*>(SetUnhandledExceptionFilter(generate_mini_dump));
}

void unset_crash_report(void* filter) { SetUnhandledExceptionFilter(reinterpret_cast<LPTOP_LEVEL_EXCEPTION_FILTER>(filter)); }

fs::path program_location(std::error_code& ec) {
    ec.clear();
    std::ignore = GetLastError();
    constexpr DWORD default_path_size = 256;
    WCHAR temp[default_path_size]{};
    GetModuleFileNameW(nullptr, temp, default_path_size);
    auto err = GetLastError();
    if (err == ERROR_SUCCESS) {
        return temp;
    }

    for (unsigned i = 2; i < 1025 && err == ERROR_INSUFFICIENT_BUFFER; i *= 2) {
        const auto size = default_path_size * i;
        std::wstring p(size, L'\0');
        const size_t len = GetModuleFileNameW(nullptr, p.data(), size);
        err = GetLastError();
        if (err == ERROR_SUCCESS) {
            p.resize(len);
            return p;
        }
    }

    ec.assign(static_cast<int>(err), std::system_category());
    return {};
}

#elif defined(macintosh) || defined(Macintosh) || (defined(__APPLE__) && defined(__MACH__))  // mac ios

static int tid_impl() {
    uint64_t tid;
    pthread_threadid_np(nullptr, &tid);
    return static_cast<int>(tid);
}

int tid() {
    static thread_local const auto v = tid_impl();
    return v;
}

fs::path program_location(std::error_code &ec) {
    ec.clear();
    constexpr uint32_t default_path_size = 256;
    char path[default_path_size];
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) == 0) {
        return path;
    }

    if (size > default_path_size) {
        std::string p(size, L'\0');
        if (_NSGetExecutablePath(p.data(), &size) == 0) {
            return p;
        }
    }

    ec = std::make_error_code(std::errc::bad_file_descriptor);
    return {};
}

#else  // linux android

int tid() {
    static thread_local const auto v = ::syscall(SYS_gettid);
    return v;
}

fs::path program_location(std::error_code &ec) { return fs::read_symlink("/proc/self/exe", ec); }

#endif

}  // namespace simple::os
