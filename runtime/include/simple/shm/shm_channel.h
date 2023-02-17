#pragma once

#include <simple/shm/shm_buffer.h>

#include <memory>
#include <simple/containers/buffer.hpp>
#include <simple/coro/task.hpp>

namespace simple {

class shm_channel_select;

/**
 * uint32:len + data
 */

class shm_channel {
  public:
    SIMPLE_API shm_channel(std::string_view src, std::string_view dst, size_t size);

    shm_channel(const shm_channel&) = delete;

    SIMPLE_API shm_channel(shm_channel&& other) noexcept;

    shm_channel& operator=(const shm_channel&) = delete;

    SIMPLE_API shm_channel& operator=(shm_channel&& other) noexcept;

    ~shm_channel() noexcept = default;

    /**
     * \brief 尝试写入数据
     * \param buf 要写入的数据
     * \param len 要写入的数据长度
     * \return 是否成功写入
     */
    SIMPLE_API bool try_write(const void* buf, uint32_t len);

    /**
     * \brief 尝试读出数据
     * \param buf 读的缓冲区
     * \param size 读的缓冲区size
     * \return 如果 <= size 表示已读出的字节数，否则表示缓冲区不够，返回实际需要的大小
     */
    SIMPLE_API uint32_t try_read(void* buf, uint32_t size);

    SIMPLE_API task<> write(const void* buf, uint32_t len);

    SIMPLE_API task<memory_buffer_ptr> read();

    SIMPLE_API task<> read(memory_buffer& buf);

  private:
    friend class shm_channel_select;
    std::unique_ptr<shm_buffer> write_;
    std::unique_ptr<shm_buffer> read_;
};

}  // namespace simple
