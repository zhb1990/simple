#pragma once
#include <simple/shm/shm.h>

namespace simple {

class shm_buffer {
  public:
    SIMPLE_API shm_buffer(std::string_view name, size_t size);

    shm_buffer(const shm_buffer&) = delete;

    SIMPLE_API shm_buffer(shm_buffer&& other) noexcept;

    ~shm_buffer() noexcept = default;

    shm_buffer& operator=(const shm_buffer&) = delete;

    SIMPLE_API shm_buffer& operator=(shm_buffer&& other) noexcept;

    [[nodiscard]] SIMPLE_API size_t readable() const;

    [[nodiscard]] SIMPLE_API size_t writable() const;

    /**
     * \brief 写入数据
     * \param buf 要写入的数据地址
     * \param len 要写入的数据长度
     * \return 返回是否成功写入
     */
    SIMPLE_API bool write(const void* buf, size_t len);

    /**
     * \brief 设置已写
     * \param len 已写的字节数
     */
    SIMPLE_API void write(size_t len);

    /**
     * \brief 填充入数据，不改变写索引
     * \param buf 要填充的数据地址
     * \param len 要填充的数据长度
     * \param offset 相对当前写索引的偏移
     * \return 返回是否成功填充
     */
    SIMPLE_API bool fill(const void* buf, size_t len, size_t offset = 0);

    /**
     * \brief 读出数据
     * \param buf 读出的缓冲区地址
     * \param len 读出的缓冲区大小
     * \return 实际读到的长度
     */
    SIMPLE_API size_t read(void* buf, size_t len);

    /**
     * \brief 设置已读
     * \param len 已读的字节数
     */
    SIMPLE_API void read(size_t len);

    /**
     * \brief 窥探数据，不会改变 读索引 与 总的读了多少字节
     * \param buf 读出的缓冲区地址
     * \param len 读出的缓冲区大小
     * \param offset 相对当前读索引的偏移
     * \return 实际读到的长度
     */
    SIMPLE_API size_t peek(void* buf, size_t len, size_t offset = 0) const;

  private:
    size_t read_impl(void* buf, size_t len, size_t offset, size_t& index) const;

    bool write_impl(const void* buf, size_t len, size_t offset, size_t& index) const;

    struct shm_data {
        // 总的读了多少字节
        size_t read;
        // 总的写了多少字节
        size_t write;
        // 读索引
        size_t read_index;
        // 写索引
        size_t write_index;
        char data[1];
    };

    shm shm_;
    shm_data* data_{nullptr};
    size_t size_{0};
};

}  // namespace simple
