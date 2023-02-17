#include <simple/shm/shm_channel_select.h>

namespace simple {

shm_channel_select& shm_channel_select::instance() {
    static shm_channel_select ins;
    return ins;
}

void shm_channel_select::start() {
    thread_ = std::jthread([this](const std::stop_token& token) { return run(token); });
}

void shm_channel_select::join() {
    if (thread_.joinable()) {
        thread_.join();
    }
}

void shm_channel_select::stop() { thread_.request_stop(); }

simple::task<> shm_channel_select::wait(shm_channel* channel, bool is_read_or_write, size_t size) {
    async_session_awaiter<void> awaiter;
    {
        std::unique_lock lock(mutex_);
        queue_.emplace_back(channel, awaiter.get_async_session(), is_read_or_write, false, size);
        cv_.notify_one();
    }
    co_await awaiter;
}

bool shm_channel_select::is_ready(const shm_channel_select::select_data& data) {
    if (data.is_read_or_write) {
        return data.channel->read_->readable() >= data.size;
    }

    return data.channel->write_->writable() >= data.size;
}

void shm_channel_select::run(const std::stop_token& token) {
    using namespace std::chrono_literals;
    size_t cnt_loop = 0;
    while (!token.stop_requested()) {
        std::deque<select_data> temp;
        if (select_.empty()) {
            std::unique_lock lock(mutex_);
            if (cv_.wait(lock, token, [this]() { return !queue_.empty(); })) {
                temp = std::move(queue_);
            }
        } else {
            // 处理需要检查的通道
            const auto size = select_.size();
            size_t write_index = size;
            for (size_t i = 0; i < size; ++i) {
                auto& data = select_[i];
                if (is_ready(data)) {
                    data.session.set_result();
                    if (write_index == size) {
                        write_index = i;
                    }
                } else if (write_index < size) {
                    select_[write_index++] = std::move(data);
                }
            }

            if (write_index != size) {
                select_.resize(write_index);
            }

            ++cnt_loop;
            if ((cnt_loop & 0x3f) == 0) {
                std::unique_lock lock(mutex_);
                if (cv_.wait_for(lock, token, 1ms, [this]() { return !queue_.empty(); })) {
                    if (!queue_.empty()) {
                        temp = std::move(queue_);
                    }
                }
            } else if ((cnt_loop & 0xf) == 0) {
                std::unique_lock lock(mutex_);
                if (cv_.wait_for(lock, token, 0ms, [this]() { return !queue_.empty(); })) {
                    if (!queue_.empty()) {
                        temp = std::move(queue_);
                    }
                }
            } else {
                std::this_thread::yield();
                {
                    std::unique_lock lock(mutex_);
                    if (!queue_.empty()) {
                        temp = std::move(queue_);
                    }
                }
            }
        }

        for (auto& data : temp) {
            if (is_ready(data)) {
                data.session.set_result();
            } else {
                select_.emplace_back(std::move(data));
            }
        }
    }
}

}  // namespace simple
