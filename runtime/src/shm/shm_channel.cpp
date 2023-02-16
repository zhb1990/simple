#include <simple/application/frame_awaiter.h>
#include <simple/shm/shm_channel.h>

#include "shm_channel_select.h"

namespace simple {

shm_channel::shm_channel(std::string_view src, std::string_view dst, size_t size) {
    std::string temp;
    temp.append(src);
    temp.append("->");
    temp.append(dst);
    write_ = std::make_unique<shm_buffer>(temp, size);

    temp.clear();
    temp.append(dst);
    temp.append("->");
    temp.append(src);
    read_ = std::make_unique<shm_buffer>(temp, size);
}

shm_channel::shm_channel(shm_channel&& other) noexcept : write_(std::move(other.write_)), read_(std::move(other.read_)) {}

shm_channel& shm_channel::operator=(shm_channel&& other) noexcept {
    if (this != &other) {
        write_ = std::move(other.write_);
        read_ = std::move(other.read_);
    }

    return *this;
}

// ReSharper disable once CppMemberFunctionMayBeConst
bool shm_channel::try_write(const void* buf, uint32_t len) {
    if (write_->writable() < len + sizeof(uint32_t)) {
        return false;
    }

    write_->fill(&len, sizeof(len));
    write_->fill(buf, len, sizeof(len));
    write_->write(len + sizeof(len));
    return true;
}

// ReSharper disable once CppMemberFunctionMayBeConst
uint32_t shm_channel::try_read(void* buf, uint32_t size) {
    uint32_t len = 0;
    if (read_->peek(&len, sizeof(len)) < sizeof(len)) {
        return 0;
    }

    if (read_->readable() < sizeof(len) + len) {
        return 0;
    }

    if (!buf) {
        return len;
    }

    if (size < len) {
        return 0;
    }

    read_->peek(buf, len, sizeof(len));
    read_->read(len + sizeof(len));

    return len;
}

task<> shm_channel::write(const void* buf, uint32_t len) {
    if (try_write(buf, len)) {
        co_return;
    }

    auto& instance = shm_channel_select::instance();
    const auto need_writable = sizeof(len) + len;

    for (;;) {
        co_await instance.wait(this, false, need_writable);
        if (try_write(buf, len)) {
            co_return;
        }
    }
}

task<memory_buffer_ptr> shm_channel::read() {
    auto result = std::make_shared<memory_buffer>();
    co_await read(*result);
    co_return result;
}

// ReSharper disable once CppMemberFunctionMayBeConst
task<> shm_channel::read(memory_buffer& buf) {
    auto& instance = shm_channel_select::instance();
    buf.clear();
    uint32_t len = 0;
    if (read_->peek(&len, sizeof(len)) < sizeof(len)) {
        for (;;) {
            co_await instance.wait(this, true, sizeof(len));
            if (read_->peek(&len, sizeof(len)) == sizeof(len)) {
                break;
            }
        }
    }

    const auto need_readable = sizeof(len) + len;
    if (read_->readable() < need_readable) {
        for (;;) {
            co_await instance.wait(this, true, need_readable);
            if (read_->readable() >= need_readable) {
                break;
            }
        }
    }

    buf.reserve(len);
    read_->peek(buf.begin_write(), len, sizeof(len));
    read_->read(sizeof(len) + len);
    buf.written(len);
}

}  // namespace simple
