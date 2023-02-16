#include <simple/shm/shm.h>

#if defined(_WIN32)
#include <Windows.h>
#else
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>

#include <filesystem>
#endif

namespace simple {

#if defined(_WIN32)

struct shm_impl {
    HANDLE handle{nullptr};
};

shm::shm(std::string_view name, size_t size) {
    impl_ = new shm_impl;

    const auto high = static_cast<DWORD>((size & 0xffffffff00000000ull) >> 32);
    const auto low = static_cast<DWORD>(size & 0xffffffffu);
    impl_->handle = CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, high, low, name.data());
    if (!impl_->handle) {
        return;
    }

    if (GetLastError() != ERROR_ALREADY_EXISTS) {
        is_create_ = true;
    }

    data_ = MapViewOfFile(impl_->handle, FILE_MAP_ALL_ACCESS, 0, 0, size);
    if (data_) {
        size_ = size;
    }
}

shm::~shm() noexcept {
    if (data_) {
        UnmapViewOfFile(data_);
    }

    if (impl_->handle) {
        CloseHandle(impl_->handle);
    }

    delete impl_;
}

shm::shm(shm&& other) noexcept : impl_(other.impl_), data_(other.data_), size_(other.size_), is_create_(other.is_create_) {
    other.impl_ = nullptr;
    other.data_ = nullptr;
    other.size_ = 0;
    other.is_create_ = false;
}

shm& shm::operator=(shm&& other) noexcept {
    if (this != &other) {
        shm temp(std::move(*this));
        std::swap(impl_, other.impl_);
        std::swap(data_, other.data_);
        std::swap(size_, other.size_);
        std::swap(is_create_, other.is_create_);
    }
    return *this;
}

// ReSharper disable once CppMemberFunctionMayBeStatic
std::error_code shm::get_error_code() {  // NOLINT(readability-convert-member-functions-to-static)
    return {static_cast<int>(GetLastError()), std::system_category()};
}

#else

struct shm_impl {
    int id{-1};
    key_t key{-1};
};

shm::shm(std::string_view name, size_t size) {
    impl_ = new shm_impl;

    std::filesystem::path p = "/tmp"sv;
    p /= name;
    std::filesystem::create_directory(p);
    impl_->key = ftok(p.c_str(), 0);
    if (impl_->key < 0) return;
    impl_->id = shmget(impl_->key, size, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    if (impl_->id < 0) {
        if (errno == EEXIST) {
            impl_->id = shmget(impl_->key, size, IPC_CREAT | S_IRUSR | S_IWUSR);
            if (impl_->id < 0) return;
        } else {
            return;
        }
    } else {
        is_create_ = true;
    }

    data_ = shmat(impl_->id, nullptr, 0);
    if (data_ == reinterpret_cast<void*>(-1)) {
        data_ = nullptr;
        return;
    }
    size_ = size;
}

shm::~shm() noexcept {
    if (data_) {
        shmdt(data_);
    }

    if (impl_->id >= 0) {
        shmctl(impl_->id, IPC_RMID, 0);
    }

    delete impl_;
}

// ReSharper disable once CppMemberFunctionMayBeStatic
std::error_code shm::get_error_code() { return {errno, std::generic_category()}; }

#endif
}  // namespace simple
