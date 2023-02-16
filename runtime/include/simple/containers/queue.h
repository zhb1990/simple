#pragma once
#include <simple/config.h>

#include <atomic>
#include <mutex>
#include <simple/utils/type_traits.hpp>

// grpc/src/core/lib/gprpp/mpscq.cc

namespace simple {

class mpsc_queue_base {
  public:
    struct node {
        std::atomic<node*> next;
    };

    DS_API mpsc_queue_base();

    DS_NON_COPYABLE(mpsc_queue_base)

    DS_API ~mpsc_queue_base() noexcept;

    // Push a node
    // Thread safe - can be called from multiple threads concurrently
    // Returns true if this was possibly the first node (may return true
    // sporadically, will not return false sporadically)
    DS_API bool push(node* n);

    // Pop a node.  Returns NULL only if the queue was empty at some point after
    // calling this function
    DS_API node* pop();

    // Pop a node; sets *empty to true if the queue is empty, or false if it is
    // not.
    DS_API node* pop_and_check_end(bool* empty);

    [[nodiscard]] DS_API size_t size() const noexcept;

  private:
    bool push_base(node* n);

    node* pop_and_check_end_base(bool* empty);

    std::atomic<node*> head_;
    [[maybe_unused]] char padding_[ds_cache_line_bytes - sizeof(head_)]{};
    node* tail_;
    node stub_;
    std::atomic_size_t size_;
};

class mpmc_queue_base {
  public:
    using node = mpsc_queue_base::node;

    // Push a node
    // Thread safe - can be called from multiple threads concurrently
    // Returns true if this was possibly the first node (may return true
    // sporadically, will not return false sporadically)
    DS_API bool push(node* n);

    // Pop a node.  Returns NULL only if the queue was empty at some point after
    // calling this function
    DS_API node* pop();

    // Pop a node (returns NULL if no node is ready - which doesn't indicate that
    // the queue is empty!!)
    // Thread compatible - can only be called from one thread at a time
    DS_API node* try_pop();

    [[nodiscard]] DS_API size_t size() const;

  private:
    mpsc_queue_base queue_;
    std::mutex mtx_;
};

template <typename Field>
concept has_mpsc_node = requires(Field t) {
                            requires class_field_type<Field>;
                            requires std::same_as<field_type<Field>, mpsc_queue_base::node>;
                        };

template <has_mpsc_node Field>
class mpsc_queue {
  public:
    using class_type = typename class_traits<Field>::class_type;
    using traits_type = class_traits<Field>;

    explicit mpsc_queue(Field field) : field_(field) {}

    DS_NON_COPYABLE(mpsc_queue)

    ~mpsc_queue() noexcept = default;

    bool push(class_type* t) { return queue_.push(&(t->*field_)); }

    class_type* pop() { return traits_type::from_field(queue_.pop()); }

    class_type* pop_and_check_end(bool* empty) { return traits_type::from_field(queue_.pop_and_check_end(empty)); }

    [[nodiscard]] size_t size() const noexcept { return queue_.size(); }

  private:
    Field field_;
    mpsc_queue_base queue_;
};

template <has_mpsc_node Field>
class mpmc_queue {
  public:
    using class_type = typename class_traits<Field>::class_type;
    using traits_type = class_traits<Field>;

    explicit mpmc_queue(Field field) : field_(field) {}

    DS_NON_COPYABLE(mpmc_queue)

    ~mpmc_queue() noexcept = default;

    bool push(class_type* t) { return queue_.push(&(t->*field_)); }

    class_type* pop() { return traits_type ::from_field(queue_.pop()); }

    class_type* try_pop() { return traits_type ::from_field(queue_.try_pop()); }

    [[nodiscard]] size_t size() const noexcept { return queue_.size(); }

  private:
    Field field_;
    mpmc_queue_base queue_;
};

}  // namespace simple
