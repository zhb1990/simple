#pragma once
#include <simple/containers/queue.h>

#include <memory>

namespace simple {

template <has_mpsc_node Field, size_t Size>
class pool {
    static_assert(Size > 0, "pool size need > 0");

  public:
    using class_type = typename class_traits<Field>::class_type;
    using traits_type = class_traits<Field>;

    explicit pool(Field field) : field_(field) {
        data_list_ = std::make_unique<class_type[]>(Size);
        for (size_t i = 0; i < Size; ++i) {
            queue_.push(&(data_list_[i].*field_));
        }
    }

    ~pool() {
        while (queue_.pop()) {
        }
    }

    DS_NON_COPYABLE(pool)

    class_type* create() {
        if (auto* n = queue_.pop()) {
            return traits_type::from_field(n, field_);
        }

        return new class_type;
    }

    void release(class_type* t) {
        if (t >= &data_list_[0] && t <= &data_list_[Size - 1]) {
            queue_.push(&(t->*field_));
            return;
        }

        delete t;
    }

  private:
    Field field_;
    std::unique_ptr<class_type[]> data_list_;
    mpmc_queue_base queue_;
};

}  // namespace simple
