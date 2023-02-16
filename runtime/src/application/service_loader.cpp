#include "service_loader.h"

#include <fmt/format.h>
#include <simple/utils/os.h>

#include <stdexcept>

#if defined(_WIN32)
#include <Windows.h>
#else
#include <dlfcn.h>
#endif

namespace simple {

#if defined(_WIN32)

static void* service_load(const char* path) {
    const auto temp = os::utf8_to_wstring(path);
    return LoadLibraryExW(temp.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
}

static void* service_api(void* m, const char* sym) {
    return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(m), sym));
}

static std::string service_error() {
    auto error = GetLastError();
    constexpr size_t buf_sz = 128;
    char buf[buf_sz]{};
    if (FormatMessageA(FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM, nullptr, error, 0, buf, buf_sz, nullptr)) {
        const auto len = strnlen(buf, buf_sz);
        return {buf, len};
    }

    return fmt::format("service error {}", error);
}

static void service_unload(void* m) { FreeLibrary(static_cast<HMODULE>(m)); }

#else

static void* service_load(const char* path) { return ::dlopen(path, RTLD_NOW | RTLD_GLOBAL); }

static void* service_api(void* m, const char* sym) { return ::dlsym(m, sym); }

static std::string service_error() { return ::dlerror(); }

static void service_unload(void* m) { ::dlclose(m); }

#endif

service_dll::~service_dll() noexcept {
    if (handle_) {
        service_unload(handle_);
    }
}

service_base* service_dll::create(const toml_value_t* args) const { return create_(args); }

void service_dll::release(const service_base* service) const { return release_(service); }

service_loader& service_loader::instance() {
    static service_loader loader;
    return loader;
}

void service_loader::add_path(const std::string& utf8_path) { path_.emplace_back(utf8_path); }

const service_dll* service_loader::query(const std::string& name) {
    if (const auto it = service_dll_map_.find(name); it != service_dll_map_.end()) {
        return &it->second;
    }

    if (path_.empty()) {
#if defined(_WIN32)
        add_path("./?.dll");
#else
        add_path("./?.so");
#endif
    }

    void* handle = nullptr;
    std::string temp;
    for (std::string_view p : path_) {
        if (p.empty()) {
            continue;
        }

        const auto pos = p.find('?');
        if (pos == std::string_view::npos) continue;

        temp = p.substr(0, pos);
        temp.append(name);
        temp.append(p.substr(pos + 1));

        handle = service_load(temp.c_str());
        if (handle) break;
    }

    if (!handle) {
        throw std::logic_error(service_error());
    }

    service_dll service;
    service.handle_ = handle;

    temp = name;
    temp.append("_create");
    service.create_ = reinterpret_cast<service_create_t>(service_api(handle, temp.c_str()));
    if (!service.create_) {
        throw std::logic_error(fmt::format("service need {}", temp));
    }

    temp = name;
    temp.append("_release");
    service.release_ = reinterpret_cast<service_release_t>(service_api(handle, temp.c_str()));
    if (!service.release_) {
        throw std::logic_error(fmt::format("service need {}", temp));
    }

    auto [it, ignore] = service_dll_map_.emplace(name, std::move(service));
    return &it->second;
}

}  // namespace simple
