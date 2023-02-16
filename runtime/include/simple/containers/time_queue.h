#pragma once

#include <simple/config.h>

#include <chrono>
#include <limits>
#include <vector>

namespace simple {

// asio 的 timer_queue 改
class timer_queue {
  public:
    using clock = std::chrono::system_clock;
    using time_point = std::chrono::system_clock::time_point;
    using duration = std::chrono::system_clock::duration;

    struct node {
        size_t index{std::numeric_limits<size_t>::max()};
        time_point point;
    };

    SIMPLE_API bool remove(node* n);

    SIMPLE_API void enqueue(node* n);

    [[nodiscard]] SIMPLE_API duration wait_duration(time_point now) const;

    SIMPLE_API std::vector<node*> get_ready_timers(time_point now);

    SIMPLE_API std::vector<node*> get_all_timers();

  private:
    void up_heap(size_t index);

    void down_heap(size_t index);

    void swap_heap(std::size_t index1, std::size_t index2);

    std::vector<node*> heap_;
};

}  // namespace simple
