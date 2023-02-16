#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <memory>
#include <string_view>

namespace simple {

class read_buffer {
  public:
    constexpr read_buffer() = default;

    constexpr read_buffer(const void* ptr, size_t capacity) : ptr_(ptr), capacity_(capacity) {}

    constexpr explicit read_buffer(const std::string_view& strv) : ptr_(strv.data()), capacity_(strv.size()) {}

    constexpr read_buffer(const read_buffer&) = default;

    constexpr read_buffer(read_buffer&& other) noexcept = default;

    constexpr ~read_buffer() noexcept = default;

    constexpr read_buffer& operator=(const read_buffer&) = default;

    constexpr read_buffer& operator=(read_buffer&&) = default;

    [[nodiscard]] constexpr const uint8_t* begin() const { return static_cast<const uint8_t*>(ptr_); }

    [[nodiscard]] constexpr const uint8_t* end() const { return static_cast<const uint8_t*>(ptr_) + capacity_; }

    [[nodiscard]] constexpr const uint8_t* begin_read() const { return static_cast<const uint8_t*>(ptr_) + read_; }

    [[nodiscard]] constexpr const uint8_t* end_read() const { return end(); }

    [[nodiscard]] constexpr const void* data() const { return ptr_; }

    [[nodiscard]] constexpr size_t capacity() const { return capacity_; }

    [[nodiscard]] constexpr size_t readable() const { return capacity_ - read_; }

    inline size_t read(void* dest, size_t size) {
        size = (std::min)(readable(), size);
        std::memcpy(dest, begin_read(), size);
        read_ += size;
        return size;
    }

    constexpr read_buffer& operator+=(size_t bytes) noexcept {
        read_ = (std::min)(read_ + bytes, capacity_);
        return *this;
    }

    constexpr explicit operator std::string_view() const noexcept {
        if (const auto sz = readable(); sz > 0) {
            return {reinterpret_cast<const char*>(begin_read()), sz};
        }

        return {};
    }

  private:
    const void* ptr_{nullptr};
    size_t capacity_{0};
    size_t read_{0};
};

class buffer {
  public:
    constexpr buffer() = default;

    constexpr buffer(void* ptr, size_t capacity, size_t prependable = 0) : data_(ptr), capacity_(capacity) {
        if (prependable <= capacity) {
            read_ = prependable;
            write_ = prependable;
        }
    }

    buffer(const buffer&) = delete;

    constexpr buffer(buffer&& other) noexcept
        : data_(other.data_), capacity_(other.capacity_), read_(other.read_), write_(other.write_) {
        other.data_ = nullptr;
        other.capacity_ = 0;
        other.read_ = 0;
        other.write_ = 0;
    }

    constexpr virtual ~buffer() noexcept = default;

    buffer& operator=(const buffer&) = delete;

    constexpr buffer& operator=(buffer&& other) noexcept {
        if (this != std::addressof(other)) {
            data_ = other.data_;
            other.data_ = nullptr;
            capacity_ = other.capacity_;
            other.capacity_ = 0;
            read_ = other.read_;
            other.read_ = 0;
            write_ = other.write_;
            other.write_ = 0;
        }

        return *this;
    }

    constexpr uint8_t* begin() { return static_cast<uint8_t*>(data_); }

    constexpr uint8_t* end() { return static_cast<uint8_t*>(data_) + capacity_; }

    constexpr uint8_t* begin_read() { return static_cast<uint8_t*>(data_) + read_; }

    constexpr uint8_t* end_read() { return static_cast<uint8_t*>(data_) + write_; }

    // ReSharper disable once CppMemberFunctionMayBeConst
    constexpr uint8_t* begin_write() { return static_cast<uint8_t*>(data_) + write_; }

    // ReSharper disable once CppMemberFunctionMayBeConst
    constexpr uint8_t* end_write() { return static_cast<uint8_t*>(data_) + capacity_; }

    constexpr void* data() { return data_; }

    [[nodiscard]] constexpr const uint8_t* begin() const { return static_cast<const uint8_t*>(data_); }

    [[nodiscard]] constexpr const uint8_t* end() const { return static_cast<const uint8_t*>(data_) + capacity_; }

    [[nodiscard]] constexpr const uint8_t* begin_read() const { return static_cast<const uint8_t*>(data_) + read_; }

    [[nodiscard]] constexpr const uint8_t* end_read() const { return static_cast<uint8_t*>(data_) + write_; }

    [[nodiscard]] constexpr const void* data() const { return data_; }

    [[nodiscard]] constexpr size_t readable() const { return write_ - read_; }

    [[nodiscard]] constexpr size_t writable() const { return capacity_ - write_; }

    [[nodiscard]] constexpr size_t capacity() const { return capacity_; }

    [[nodiscard]] constexpr size_t prependable() const { return read_; }

    void shrink() noexcept {
        if (read_ == 0) {
            return;
        }

        const auto size = readable();
        std::memmove(data_, begin_read(), size);
        read_ = 0;
        write_ = size;
    }

    constexpr explicit operator read_buffer() const noexcept {
        const auto size = readable();
        if (size == 0) {
            return {};
        }

        return {begin_read(), size};
    }

    constexpr explicit operator std::string_view() const noexcept {
        const auto size = readable();
        if (size == 0) {
            return {};
        }

        return {reinterpret_cast<const char*>(begin_read()), size};
    }

    constexpr void clear(size_t prependable = 0) noexcept {
        prependable = (std::min)(prependable, capacity_);
        read_ = prependable;
        write_ = prependable;
    }

    inline virtual void append(const void* buf, size_t len) {
        len = (std::min)(len, writable());
        std::memmove(begin_write(), buf, len);
        written(len);
    }

    inline void append(const read_buffer& buf) { return append(buf.begin_read(), buf.readable()); }

    /**
     * \brief 从begin_read顺序拷贝内容到buf中
     * \param buf 拷贝的目的地
     * \param sz 拷贝的目的地的最大长度
     * \return 拷贝的数据大小
     */
    [[nodiscard]] size_t peek(void* buf, size_t sz) const noexcept {
        sz = (std::min)(sz, readable());
        memmove(buf, begin_read(), sz);
        return sz;
    }

    /**
     * \brief 从end_read前sz字节，拷贝内容到buf中
     * \param buf 拷贝的目的地
     * \param sz 拷贝的目的地的最大长度
     * \return 拷贝的数据大小
     */
    [[nodiscard]] size_t rpeek(void* buf, size_t sz) const noexcept {
        sz = (std::min)(sz, readable());
        memmove(buf, end_read() - sz, sz);
        return sz;
    }

    bool prepend(const void* buf, size_t len) noexcept {
        if (prependable() < len) {
            return false;
        }

        std::memmove(begin_read() - len, buf, len);
        read_ -= len;
        return true;
    }

    constexpr void written(size_t sz) noexcept { write_ += (std::min)(sz, writable()); }

    constexpr void read(size_t sz) noexcept { read_ += (std::min)(sz, readable()); }

    constexpr void read_until(const uint8_t* end) noexcept {
        if (end > begin_read() && end <= end_read()) {
            read_ += end - begin_read();
        }
    }

  protected:
    void* data_{nullptr};
    size_t capacity_{0};
    size_t read_{0};
    size_t write_{0};
};

template <typename Allocator>
concept memory_buffer_allocator = requires {
                                      typename std::allocator_traits<Allocator>::value_type;
                                      requires std::same_as<typename std::allocator_traits<Allocator>::value_type, uint8_t>;
                                  };

template <size_t Fixed, memory_buffer_allocator Allocator>
class base_memory_buffer final : public buffer {
  public:
    using allocator_traits = std::allocator_traits<Allocator>;

    explicit base_memory_buffer(size_t prependable = 0, const Allocator& alloc = Allocator());

    base_memory_buffer(const base_memory_buffer& other);

    template <size_t FixedOther, memory_buffer_allocator AllocatorOther>
    explicit base_memory_buffer(const base_memory_buffer<FixedOther, AllocatorOther>& other);

    base_memory_buffer(base_memory_buffer&& other) noexcept;

    explicit base_memory_buffer(const read_buffer& buf, const Allocator& alloc = Allocator());

    base_memory_buffer(const void* ptr, size_t len, const Allocator& alloc = Allocator());

    ~base_memory_buffer() noexcept override;

    void append(const void* buf, size_t len) override;

    void make_sure_writable(size_t len);

    base_memory_buffer& operator=(const base_memory_buffer& other);

    template <size_t FixedOther, memory_buffer_allocator AllocatorOther>
    base_memory_buffer& operator=(const base_memory_buffer<FixedOther, AllocatorOther>& other);

    base_memory_buffer& operator=(base_memory_buffer&& other) noexcept;

    void reserve(size_t size);

    void release();

  private:
    void grow(size_t size);

    uint8_t store_[Fixed]{};
    Allocator alloc_;
};

template <size_t Fixed, memory_buffer_allocator Allocator>
base_memory_buffer<Fixed, Allocator>::base_memory_buffer(size_t prependable, const Allocator& alloc)
    : buffer(store_, Fixed, prependable), alloc_(alloc) {}

template <size_t Fixed, memory_buffer_allocator Allocator>
base_memory_buffer<Fixed, Allocator>::base_memory_buffer(const base_memory_buffer& other)
    : base_memory_buffer(0, other.alloc_) {
    reserve(other.capacity_);
    read_ = other.read_;
    write_ = other.write_;
    std::memcpy(data_, other.data_, write_);
}

template <size_t Fixed, memory_buffer_allocator Allocator>
template <size_t FixedOther, memory_buffer_allocator AllocatorOther>
base_memory_buffer<Fixed, Allocator>::base_memory_buffer(const base_memory_buffer<FixedOther, AllocatorOther>& other)
    : base_memory_buffer(0, other.alloc_) {
    reserve(other.capacity_);
    read_ = other.read_;
    write_ = other.write_;
    std::memcpy(data_, other.data_, write_);
}

template <size_t Fixed, memory_buffer_allocator Allocator>
base_memory_buffer<Fixed, Allocator>::base_memory_buffer(base_memory_buffer&& other) noexcept : alloc_(other.alloc_) {
    std::swap(capacity_, other.capacity_);
    std::swap(read_, other.read_);
    std::swap(write_, other.write_);

    if (other.data_ != other.store_) {
        data_ = other.data_;
        other.data_ = other.store_;
    } else {
        std::memcpy(store_, other.store_, write_);
        data_ = store_;
    }
}

template <size_t Fixed, memory_buffer_allocator Allocator>
base_memory_buffer<Fixed, Allocator>::base_memory_buffer(const read_buffer& buf, const Allocator& alloc)
    : base_memory_buffer(0, alloc) {
    append(buf);
}

template <size_t Fixed, memory_buffer_allocator Allocator>
base_memory_buffer<Fixed, Allocator>::base_memory_buffer(const void* ptr, size_t len, const Allocator& alloc)
    : base_memory_buffer(0, alloc) {
    append(ptr, len);
}

template <size_t Fixed, memory_buffer_allocator Allocator>
base_memory_buffer<Fixed, Allocator>::~base_memory_buffer() noexcept {
    if (data_ != store_) {
        alloc_.deallocate(static_cast<uint8_t*>(data_), capacity_);
    }
}

template <size_t Fixed, memory_buffer_allocator Allocator>
void base_memory_buffer<Fixed, Allocator>::make_sure_writable(size_t len) {
    if (const auto sz = writable(); sz < len) {
        grow(capacity_ + len - sz);
    }
}

template <size_t Fixed, memory_buffer_allocator Allocator>
void base_memory_buffer<Fixed, Allocator>::append(const void* buf, size_t len) {
    make_sure_writable(len);
    return buffer::append(buf, len);
}

template <size_t Fixed, memory_buffer_allocator Allocator>
base_memory_buffer<Fixed, Allocator>&
base_memory_buffer<Fixed, Allocator>::operator=(  // NOLINT(bugprone-unhandled-self-assignment)
    const base_memory_buffer& other) {
    if (this != std::addressof(other)) {
        reserve(other.capacity_);
        read_ = other.read_;
        write_ = other.write_;
        std::memcpy(data_, other.data_, write_);
    }

    return *this;
}

template <size_t Fixed, memory_buffer_allocator Allocator>
template <size_t FixedOther, memory_buffer_allocator AllocatorOther>
base_memory_buffer<Fixed, Allocator>& base_memory_buffer<Fixed, Allocator>::operator=(
    const base_memory_buffer<FixedOther, AllocatorOther>& other) {
    if (this != std::addressof(other)) {
        reserve(other.capacity_);
        read_ = other.read_;
        write_ = other.write_;
        std::memcpy(data_, other.data_, write_);
    }

    return *this;
}

template <size_t Fixed, memory_buffer_allocator Allocator>
base_memory_buffer<Fixed, Allocator>& base_memory_buffer<Fixed, Allocator>::operator=(base_memory_buffer&& other) noexcept {
    if (this != std::addressof(other)) {
        [[maybe_unused]] base_memory_buffer temp = std::move(*this);
        std::swap(alloc_, other.alloc_);
        std::swap(capacity_, other.capacity_);
        std::swap(read_, other.read_);
        std::swap(write_, other.write_);

        if (other.data_ != other.store_) {
            data_ = other.data_;
            other.data_ = other.store_;
        } else {
            std::memcpy(store_, other.store_, write_);
        }
    }

    return *this;
}

template <size_t Fixed, memory_buffer_allocator Allocator>
void base_memory_buffer<Fixed, Allocator>::reserve(size_t size) {
    if (size > capacity_) {
        grow(size);
    }
}

template <size_t Fixed, memory_buffer_allocator Allocator>
void base_memory_buffer<Fixed, Allocator>::release() {
    if (data_ != store_) {
        alloc_.deallocate(static_cast<uint8_t*>(data_), capacity_);
        data_ = store_;
        capacity_ = Fixed;
    }

    clear();
}

template <size_t Fixed, memory_buffer_allocator Allocator>
void base_memory_buffer<Fixed, Allocator>::grow(size_t size) {
    const size_t max_size = allocator_traits::max_size(alloc_);
    size_t new_capacity = capacity_ + capacity_ / 2;
    if (size > new_capacity) {
        new_capacity = size;
    } else if (new_capacity > max_size) {
        new_capacity = size > max_size ? size : max_size;
    }

    auto* old_data = data_;
    const auto old_capacity = capacity_;

    uint8_t* new_data = allocator_traits::allocate(alloc_, new_capacity);
    std::memcpy(new_data, old_data, write_);

    data_ = new_data;
    capacity_ = new_capacity;

    if (old_data != store_) {
        alloc_.deallocate(static_cast<uint8_t*>(old_data), old_capacity);
    }
}

using memory_buffer = base_memory_buffer<1024, std::allocator<uint8_t>>;

using memory_buffer_ptr = std::shared_ptr<memory_buffer>;

}  // namespace simple

template <size_t Fixed, simple::memory_buffer_allocator Allocator>
class std::back_insert_iterator<simple::base_memory_buffer<Fixed, Allocator>> {
  public:
    using iterator_category = std::output_iterator_tag;
    using value_type = void;
    using pointer = void;
    using reference = void;

    using container_type = simple::base_memory_buffer<Fixed, Allocator>;

#ifdef __cpp_lib_concepts
    using difference_type = ptrdiff_t;
#else
    using difference_type = void;
#endif  // __cpp_lib_concepts

    explicit back_insert_iterator(container_type& buf) noexcept : container(std::addressof(buf)) {}

    back_insert_iterator& operator=(const char& val) {
        container->append(&val, 1);
        return *this;
    }

    back_insert_iterator& operator=(char&& val) {
        container->append(&val, 1);
        val = 0;
        return *this;
    }

    back_insert_iterator& operator*() noexcept { return *this; }

    back_insert_iterator& operator++() noexcept { return *this; }

    back_insert_iterator operator++(int) noexcept { return *this; }

  protected:
    container_type* container;
};
