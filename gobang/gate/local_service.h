#pragma once
#include <simple/coro/condition_variable.h>
#include <simple/shm/shm_channel.h>

#include <deque>

#include "service_info.h"

struct local_service final : service_info {
    std::unique_ptr<simple::shm_channel> channel;
    std::deque<simple::memory_buffer> send_queue;
    simple::condition_variable cv_send_queue;
    uint32_t socket{0};

    void write(const std::string_view& message) override;

    void start();

    simple::task<> auto_write();

    [[nodiscard]] simple::task<> auto_read() const;
};
