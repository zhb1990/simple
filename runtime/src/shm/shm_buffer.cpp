#include <simple/shm/shm_buffer.h>

namespace simple {

shm_buffer::shm_buffer(std::string_view name, size_t size) : shm_(name, size + sizeof(size_t) * 4), size_(size) {
    data_ = static_cast<shm_data*>(shm_.data());
    if (shm_.is_create()) {
        memset(data_, 0, size + sizeof(size_t) * 4);
    }
}

shm_buffer::shm_buffer(shm_buffer&& other) noexcept : shm_(std::move(other.shm_)), data_(other.data_), size_(other.size_) {
    other.data_ = nullptr;
    other.size_ = 0;
}

shm_buffer& shm_buffer::operator=(shm_buffer&& other) noexcept {
    if (this != &other) {
        shm_buffer temp(std::move(*this));
        shm_ = std::move(other.shm_);
        std::swap(data_, other.data_);
        std::swap(size_, other.size_);
    }

    return *this;
}

size_t shm_buffer::readable() const { return data_->write - data_->read; }

size_t shm_buffer::writable() const { return size_ + data_->read - data_->write; }

bool shm_buffer::write(const void* buf, size_t len) {
    size_t index{0};
    if (!write_impl(buf, len, 0, index)) {
        return false;
    }

    data_->write_index = index;
    data_->write += len;
    return true;
}

void shm_buffer::write(size_t len) {
    if (len > writable()) {
        return;
    }

    if (auto temp = size_ - data_->write_index; len > temp) {
        temp = len - temp;
        data_->write_index = temp;
    } else {
        data_->write_index += len;
        if (data_->write_index == size_) {
            data_->write_index = 0;
        }
    }

    data_->write += len;
}

bool shm_buffer::fill(const void* buf, size_t len, size_t offset) {
    size_t index{0};
    return write_impl(buf, len, offset, index);
}

size_t shm_buffer::read(void* buf, size_t len) {
    size_t index{0};
    len = read_impl(buf, len, 0, index);
    if (len > 0) {
        data_->read_index = index;
        data_->read += len;
    }

    return len;
}

void shm_buffer::read(size_t len) {
    len = std::min(len, readable());
    if (len == 0) {
        return;
    }

    auto index = data_->read_index;
    if (auto temp = size_ - index; len > temp) {
        temp = len - temp;
        index = temp;
    } else {
        index += len;
        if (index == size_) {
            index = 0;
        }
    }

    data_->read_index = index;
    data_->read += len;
}

size_t shm_buffer::peek(void* buf, size_t len, size_t offset) const {
    size_t index{0};
    return read_impl(buf, len, offset, index);
}

size_t shm_buffer::read_impl(void* buf, size_t len, size_t offset, size_t& index) const {
    const auto total = readable();
    if (total <= offset) {
        return 0;
    }

    len = std::min(len, total - offset);
    if (len == 0) {
        return 0;
    }

    index = data_->read_index + offset;
    if (index >= size_) {
        index -= size_;
    }

    if (auto temp = size_ - index; len > temp) {
        // 需要分成两段读
        memcpy(buf, data_->data + index, temp);
        buf = static_cast<char*>(buf) + temp;
        temp = len - temp;
        memcpy(buf, data_->data, temp);
        index = temp;
    } else {
        memcpy(buf, data_->data + index, len);
        index += len;
        if (index == size_) {
            index = 0;
        }
    }

    return len;
}

bool shm_buffer::write_impl(const void* buf, size_t len, size_t offset, size_t& index) const {
    const auto total = writable();
    if (total <= offset) {
        return false;
    }

    if (len > total - offset) {
        return false;
    }

    index = data_->write_index + offset;
    if (index >= size_) {
        index -= size_;
    }

    if (auto temp = size_ - index; len > temp) {
        // 需要分成两段写
        memmove(data_->data + index, buf, temp);
        buf = static_cast<const char*>(buf) + temp;
        temp = len - temp;
        memmove(data_->data, buf, temp);
        index = temp;
    } else {
        memmove(data_->data + index, buf, len);
        index += len;
        if (index == size_) {
            index = 0;
        }
    }

    return true;
}

}  // namespace simple
