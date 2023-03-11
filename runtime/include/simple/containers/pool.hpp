#pragma once
#include <simple/containers/queue.h>

#include <memory>

namespace simple {

template <std::derived_from<mpsc_queue::node> T, size_t Size>
class pool {
    static_assert(Size > 0, "pool size need > 0");

  public:
    explicit pool() {
        data_list_ = std::make_unique<T[]>(Size);
        for (size_t i = 0; i < Size; ++i) {
            queue_.push(&(data_list_[i]));
        }
    }

    ~pool() {
        while (queue_.pop()) {
        }
    }

    SIMPLE_NON_COPYABLE(pool)

    T* create() {
        if (auto* n = queue_.pop()) {
            return dynamic_cast<T*>(n);
        }

        return new T;
    }

    void release(T* t) {
        if (t >= &data_list_[0] && t <= &data_list_[Size - 1]) {
            queue_.push(t);
            return;
        }

        delete t;
    }

  private:
    std::unique_ptr<T[]> data_list_;
    mpmc_queue queue_;
};

}  // namespace simple
