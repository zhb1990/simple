#pragma once
#include <simple/config.h>

#include <simple/utils/type_traits.hpp>
#include <utility>

#include "heap.hpp"

namespace simple {

class heap_raw;

struct heap_node {
    heap_node* left{nullptr};
    heap_node* right{nullptr};
    heap_node* parent{nullptr};
    heap_raw* heap{nullptr};
};

template <typename T>
concept compare_heap_node = std::is_invocable_r_v<bool, T, const heap_node*, const heap_node*>;

// ReSharper disable once CommentTypo
/**
 * \brief libuv中链表实现的堆，类似标准库std::priority_queue
 */
class heap_raw {
  public:
    heap_raw() = default;

    DS_NON_COPYABLE(heap_raw)

    ~heap_raw() noexcept = default;

    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }

    [[nodiscard]] size_t size() const noexcept { return size_; }

    [[nodiscard]] heap_node* min_node() const noexcept { return min_; }

    template <compare_heap_node Compare>
    bool insert(heap_node* node, Compare compare) {
        if (node->heap) {
            return false;
        }

        node->left = nullptr;
        node->right = nullptr;
        node->parent = nullptr;

        /* Calculate the path from the root to the insertion point.  This is a min
         * heap so we always insert at the left-most free node of the bottom row.
         */
        size_t n;
        size_t k;
        size_t path = 0;
        for (k = 0, n = 1 + size_; n >= 2; k += 1, n /= 2) path = (path << 1) | (n & 1);

        /* Now traverse the heap using the path we calculated in the previous step. */
        auto* parent = &min_;
        auto* child = parent;
        while (k > 0) {
            parent = child;
            if (path & 1) {
                child = &(*child)->right;
            } else {
                child = &(*child)->left;
            }
            path >>= 1;
            k -= 1;
        }

        /* Insert the new node. */
        node->parent = *parent;
        *child = node;
        node->heap = this;
        ++size_;

        /* Walk up the tree and check at each node if the heap property holds.
         * It's a min heap so parent < child must be true.
         */
        while (node->parent && compare(node, node->parent)) {
            swap_node(node->parent, node);
        }

        return true;
    }

    template <compare_heap_node Compare>
    bool remove(heap_node* node, Compare compare) {
        if (node->heap != this) return false;
        if (empty()) return false;

        /* Calculate the path from the min (the root) to the max, the left-most node
         * of the bottom row.
         */
        size_t path = 0;
        size_t k;
        size_t n;
        for (k = 0, n = size_; n >= 2; k += 1, n /= 2) {
            path = (path << 1) | (n & 1);
        }

        /* Now traverse the heap using the path we calculated in the previous step. */
        auto* max = &min_;
        while (k > 0) {
            if (path & 1) {
                max = &(*max)->right;
            } else {
                max = &(*max)->left;
            }
            path >>= 1;
            k -= 1;
        }

        --size_;

        /* Unlink the max node. */
        auto* child = *max;
        *max = nullptr;

        if (child == node) {
            /* We're removing either the max or the last node in the tree. */
            if (child == min_) {
                min_ = nullptr;
            }
            node->heap = nullptr;
            return true;
        }

        /* Replace the to be deleted node with the max node. */
        child->left = node->left;
        child->right = node->right;
        child->parent = node->parent;

        if (child->left) {
            child->left->parent = child;
        }

        if (child->right) {
            child->right->parent = child;
        }

        if (!node->parent) {
            min_ = child;
        } else if (node->parent->left == node) {
            node->parent->left = child;
        } else {
            node->parent->right = child;
        }
        node->heap = nullptr;

        /* Walk down the subtree and check at each node if the heap property holds.
         * It's a min heap so parent < child must be true.  If the parent is bigger,
         * swap it with the smallest child.
         */
        auto* smallest = child;
        for (;;) {
            if (child->left && compare(child->left, smallest)) smallest = child->left;
            if (child->right && compare(child->right, smallest)) smallest = child->right;
            if (smallest == child) break;
            swap_node(child, smallest);  // NOLINT(readability-suspicious-call-argument)
            smallest = child;
        }

        /* Walk up the subtree and check that each parent is less than the node
         * this is required, because `max` node is not guaranteed to be the
         * actual maximum in tree
         */
        while (child->parent && compare(child, child->parent)) {
            swap_node(child->parent, child);
        }

        return true;
    }

  protected:
    void swap_node(heap_node* parent, heap_node* child) {
        const auto temp = *parent;
        *parent = *child;
        *child = temp;
        parent->parent = child;

        heap_node* sibling;
        if (child->left == child) {
            child->left = parent;
            sibling = child->right;
        } else {
            child->right = parent;
            sibling = child->left;
        }
        if (sibling) sibling->parent = child;

        if (parent->left) parent->left->parent = parent;
        if (parent->right) parent->right->parent = parent;

        if (!child->parent) {
            min_ = child;
        } else if (child->parent->left == parent) {
            child->parent->left = child;
        } else {
            child->parent->right = child;
        }
    }

    heap_node* min_{nullptr};
    size_t size_{0};
};

template <typename Field>
concept has_heap_node = requires {
                            requires class_field_type<Field>;
                            requires std::same_as<heap_node, field_type<Field>>;
                        };

template <has_heap_node Field, typename Compare = std::less<class_type<Field>>>
requires compare_able<class_type<Field>, Compare>
class heap : public heap_raw {
  public:
    using node_type = heap_node;
    using class_type = typename class_traits<Field>::class_type;
    using traits_type = class_traits<Field>;

    explicit heap(Field field) : field_(field) {}

    DS_NON_COPYABLE(heap)

    ~heap() noexcept = default;

    bool push(class_type* t) {
        node_type* node = &(t->*field_);
        return heap_raw::insert(node, [this](const node_type* left, const node_type* right) {
            return compare_(*traits_type::from_field(left, field_), *traits_type::from_field(right, field_));
        });
    }

    [[nodiscard]] class_type* front() const noexcept { return traits_type::from_field(min_, field_); }

    bool pop() {
        return heap_raw::remove(min_, [this](const node_type* left, const node_type* right) {
            return compare_(*traits_type::from_field(left, field_), *traits_type::from_field(right, field_));
        });
    }

    bool erase(class_type* t) {
        node_type* node = &(t->*field_);
        return heap_raw::remove(node, [this](const node_type* left, const node_type* right) {
            return compare_(*traits_type::from_field(left, field_), *traits_type::from_field(right, field_));
        });
    }

  private:
    Field field_;
    Compare compare_;
};

}  // namespace simple
