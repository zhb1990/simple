#pragma once
#include <simple/config.h>

#include <atomic>
#include <mutex>

// grpc/src/core/lib/gprpp/mpscq.cc

namespace simple {

class mpsc_queue {
  public:
    struct node {
        std::atomic<node*> next;
        virtual ~node() noexcept = default;
    };

    SIMPLE_API mpsc_queue();

    SIMPLE_NON_COPYABLE(mpsc_queue)

    SIMPLE_API ~mpsc_queue() noexcept;

    // Push a node
    // Thread safe - can be called from multiple threads concurrently
    // Returns true if this was possibly the first node (may return true
    // sporadically, will not return false sporadically)
    SIMPLE_API bool push(node* n);

    // Pop a node.  Returns NULL only if the queue was empty at some point after
    // calling this function
    SIMPLE_API node* pop();

    // Pop a node; sets *empty to true if the queue is empty, or false if it is
    // not.
    SIMPLE_API node* pop_and_check_end(bool* empty);

    [[nodiscard]] SIMPLE_API size_t size() const noexcept;

  private:
    bool push_base(node* n);

    node* pop_and_check_end_base(bool* empty);

    std::atomic<node*> head_;
    [[maybe_unused]] char padding_[simple_cache_line_bytes - sizeof(head_)]{};
    node* tail_;
    node stub_;
    std::atomic_size_t size_;
};

class mpmc_queue {
  public:
    using node = mpsc_queue::node;

    // Push a node
    // Thread safe - can be called from multiple threads concurrently
    // Returns true if this was possibly the first node (may return true
    // sporadically, will not return false sporadically)
    SIMPLE_API bool push(node* n);

    // Pop a node.  Returns NULL only if the queue was empty at some point after
    // calling this function
    SIMPLE_API node* pop();

    // Pop a node (returns NULL if no node is ready - which doesn't indicate that
    // the queue is empty!!)
    // Thread compatible - can only be called from one thread at a time
    SIMPLE_API node* try_pop();

    [[nodiscard]] SIMPLE_API size_t size() const;

  private:
    mpsc_queue queue_;
    std::mutex mtx_;
};

}  // namespace simple
