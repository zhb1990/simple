#include <simple/containers/time_queue.h>

namespace simple {

bool timer_queue::remove(node* n) {
    // Remove the timer from the heap.
    if (const auto index = n->index; !heap_.empty() && index < heap_.size()) {
        if (index == heap_.size() - 1) {
            n->index = std::numeric_limits<size_t>::max();
            heap_.pop_back();
        } else {
            swap_heap(index, heap_.size() - 1);
            n->index = std::numeric_limits<size_t>::max();
            heap_.pop_back();
            if (index > 0 && heap_[index]->point < heap_[(index - 1) / 2]->point) {
                up_heap(index);
            } else {
                down_heap(index);
            }
        }

        return true;
    }

    return false;
}

void timer_queue::enqueue(node* n) {
    const auto size = heap_.size();
    if (n->index < size) {
        return;
    }

    // check point
    if (constexpr time_point zero; n->point < zero) {
        n->point = zero;
    }

    // Put the new timer at the correct position in the heap. This is done
    // first since push_back() can throw due to allocation failure.
    n->index = size;
    heap_.push_back(n);
    up_heap(size);
}

timer_queue::duration timer_queue::wait_duration(time_point now) const {
    if (heap_.empty()) {
        return duration::max();
    }

    return heap_[0]->point - now;
}

std::vector<timer_queue::node*> timer_queue::get_ready_timers(time_point now) {
    std::vector<node*> result;
    while (!heap_.empty() && now >= heap_[0]->point) {
        auto* n = heap_[0];
        result.emplace_back(n);
        remove(n);
    }

    return result;
}

std::vector<timer_queue::node*> timer_queue::get_all_timers() { return std::move(heap_); }

void timer_queue::up_heap(size_t index) {
    while (index > 0) {
        const auto parent = (index - 1) / 2;
        if (heap_[index]->point >= heap_[parent]->point) {
            break;
        }

        swap_heap(index, parent);
        index = parent;
    }
}

void timer_queue::down_heap(size_t index) {
    size_t child = index * 2 + 1;
    while (child < heap_.size()) {
        const auto min_child = (child + 1 == heap_.size() || heap_[child]->point < heap_[child + 1]->point) ? child : child + 1;
        if (heap_[index]->point < heap_[min_child]->point) {
            break;
        }

        swap_heap(index, min_child);
        index = min_child;
        child = index * 2 + 1;
    }
}

void timer_queue::swap_heap(std::size_t index1, std::size_t index2) {
    auto& t1 = heap_[index1];
    auto& t2 = heap_[index2];
    std::swap(t1, t2);
    t1->index = index1;
    t2->index = index2;
}

}  // namespace simple
