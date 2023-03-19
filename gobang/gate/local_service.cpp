#include "local_service.h"

#include <simple/application/service.hpp>
#include <simple/coro/co_start.hpp>

void local_service::write(const std::string_view& message) {
    // 发送队列中有数据，或者写入失败说明共享内存写满了
    if (!send_queue.empty() || !channel->try_write(message.data(), message.size())) {
        // 放入发送队列
        send_queue.emplace_back(message.data(), message.size());
        cv_send_queue.notify_all();
    }
}

void local_service::start() {
    simple::co_start([this]() { return auto_write(); });

    simple::co_start([this]() { return auto_read(); });
}

simple::task<> local_service::auto_write() {
    for (;;) {
        if (send_queue.empty()) {
            co_await cv_send_queue.wait();
            continue;
        }

        const auto& message = send_queue.front();
        co_await channel->write(message.begin_read(), static_cast<uint32_t>(message.readable()));
        send_queue.pop_front();
    }
}

simple::task<> local_service::auto_read() const {
    simple::memory_buffer buf;
    auto& events = service->events();
    forward_message_event ev;
    for (;;) {
        co_await channel->read(buf);
        ev.strv = std::string_view(buf);
        events.fire_event(ev);
    }
}
