#include <simple/containers/queue.h>

#include <cassert>

namespace simple {

mpsc_queue_base::mpsc_queue_base() : head_{&stub_}, tail_(&stub_) {}

mpsc_queue_base::~mpsc_queue_base() noexcept {
    assert(head_.load(std::memory_order::relaxed) == &stub_);
    assert(tail_ == &stub_);
}

bool mpsc_queue_base::push(node* n) {
    size_.fetch_add(1, std::memory_order::relaxed);
    return push_base(n);
}

mpsc_queue_base::node* mpsc_queue_base::pop() {
    bool empty = false;
    node* n;
    do {
        n = pop_and_check_end_base(&empty);
    } while (n == nullptr && !empty);

    if (n) {
        size_.fetch_sub(1, std::memory_order::relaxed);
    }
    return n;
}

mpsc_queue_base::node* mpsc_queue_base::pop_and_check_end(bool* empty) {
    auto* n = pop_and_check_end_base(empty);
    if (n) {
        size_.fetch_sub(1, std::memory_order::relaxed);
    }
    return n;
}

size_t mpsc_queue_base::size() const noexcept { return size_.load(std::memory_order::relaxed); }

bool mpsc_queue_base::push_base(node* n) {
    n->next.store(nullptr, std::memory_order::relaxed);
    node* prev = head_.exchange(n, std::memory_order::acq_rel);
    prev->next.store(n, std::memory_order::release);
    return prev == &stub_;
}

mpsc_queue_base::node* mpsc_queue_base::pop_and_check_end_base(bool* empty) {
    node* tail = tail_;
    node* next = tail_->next.load(std::memory_order::acquire);
    if (tail == &stub_) {
        // indicates the list is actually (ephemerally) empty
        if (next == nullptr) {
            *empty = true;
            return nullptr;
        }
        tail_ = next;
        tail = next;
        next = tail->next.load(std::memory_order::acquire);
    }

    if (next != nullptr) {
        *empty = false;
        tail_ = next;
        return tail;
    }

    if (tail != head_.load(std::memory_order::acquire)) {
        *empty = false;
        // indicates a retry is in order: we're still adding
        return nullptr;
    }

    push_base(&stub_);
    next = tail->next.load(std::memory_order::acquire);
    if (next != nullptr) {
        *empty = false;
        tail_ = next;
        return tail;
    }

    // indicates a retry is in order: we're still adding
    *empty = false;
    return nullptr;
}

bool mpmc_queue_base::push(node* n) { return queue_.push(n); }

mpmc_queue_base::node* mpmc_queue_base::pop() {
    std::unique_lock lock(mtx_);
    return queue_.pop();
}

mpmc_queue_base::node* mpmc_queue_base::try_pop() {
    if (mtx_.try_lock()) {
        auto* n = queue_.pop();
        mtx_.unlock();
        return n;
    }
    return nullptr;
}

size_t mpmc_queue_base::size() const { return queue_.size(); }

}  // namespace simple
