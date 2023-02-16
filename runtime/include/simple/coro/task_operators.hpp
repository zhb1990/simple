#pragma once

#include <simple/coro/parallel_task.hpp>
#include <simple/coro/task.hpp>
#include <simple/utils/multiple_exceptions.hpp>

namespace simple {

inline task<> operator&&(task<> left, task<> right) {
    auto [l, r] = co_await wait_parallel_task_ready(parallel_task_type::wait_one_fail, std::move(left), std::move(right));
    const auto le = l.get_exception();
    const auto re = r.get_exception();

    if (le && re) {
        throw multiple_exceptions(le);
    }

    if (le) {
        std::rethrow_exception(le);
    }

    if (re) {
        std::rethrow_exception(re);
    }
}

template <typename Left>
task<Left> operator&&(task<Left> left, task<> right) {
    auto [l, r] = co_await wait_parallel_task_ready(parallel_task_type::wait_one_fail, std::move(left), std::move(right));
    const auto le = l.get_exception();
    const auto re = r.get_exception();

    if (le && re) {
        throw multiple_exceptions(le);
    }

    if (le) {
        std::rethrow_exception(le);
    }

    if (re) {
        std::rethrow_exception(re);
    }

    co_return std::move(l).result();
}

template <typename Right>
task<Right> operator&&(task<> left, task<Right> right) {
    auto [l, r] = co_await wait_parallel_task_ready(parallel_task_type::wait_one_fail, std::move(left), std::move(right));
    const auto le = l.get_exception();
    const auto re = r.get_exception();

    if (le && re) {
        throw multiple_exceptions(le);
    }

    if (le) {
        std::rethrow_exception(le);
    }

    if (re) {
        std::rethrow_exception(re);
    }

    co_return std::move(r).result();
}

template <typename Left, typename Right>
task<std::tuple<Left, Right>> operator&&(task<Left> left, task<Right> right) {
    auto [l, r] = co_await wait_parallel_task_ready(parallel_task_type::wait_one_fail, std::move(left), std::move(right));
    const auto le = l.get_exception();
    const auto re = r.get_exception();

    if (le && re) {
        throw multiple_exceptions(le);
    }

    if (le) {
        std::rethrow_exception(le);
    }

    if (re) {
        std::rethrow_exception(re);
    }

    co_return std::make_tuple(std::move(l).result(), std::move(r).result());
}

template <typename... Left>
task<std::tuple<Left...>> operator&&(task<std::tuple<Left...>> left, task<> right) {
    auto [l, r] = co_await wait_parallel_task_ready(parallel_task_type::wait_one_fail, std::move(left), std::move(right));
    const auto le = l.get_exception();
    const auto re = r.get_exception();

    if (le && re) {
        throw multiple_exceptions(le);
    }

    if (le) {
        std::rethrow_exception(le);
    }

    if (re) {
        std::rethrow_exception(re);
    }

    co_return std::move(l).result();
}

template <typename... Left, typename Right>
task<std::tuple<Left..., Right>> operator&&(task<std::tuple<Left...>> left, task<Right> right) {
    auto [l, r] = co_await wait_parallel_task_ready(parallel_task_type::wait_one_fail, std::move(left), std::move(right));
    const auto le = l.get_exception();
    const auto re = r.get_exception();

    if (le && re) {
        throw multiple_exceptions(le);
    }

    if (le) {
        std::rethrow_exception(le);
    }

    if (re) {
        std::rethrow_exception(re);
    }

    co_return std::tuple_cat(std::move(l).result(), std::make_tuple(std::move(r).result()));
}

inline task<std::variant<std::monostate, std::monostate>> operator||(task<> left, task<> right) {
    auto [l, r] = co_await wait_parallel_task_ready(parallel_task_type::wait_one_succ, std::move(left), std::move(right));
    const auto le = l.get_exception();
    const auto re = r.get_exception();

    if (l.get_index() == 0) {
        if (!le) {
            co_return std::variant<std::monostate, std::monostate>{std::in_place_index<0>};
        }

        if (!re) {
            co_return std::variant<std::monostate, std::monostate>{std::in_place_index<1>};
        }

        throw multiple_exceptions(le);
    }

    if (!re) {
        co_return std::variant<std::monostate, std::monostate>{std::in_place_index<1>};
    }

    if (!le) {
        co_return std::variant<std::monostate, std::monostate>{std::in_place_index<0>};
    }

    throw multiple_exceptions(re);
}

template <typename Left>
task<std::variant<Left, std::monostate>> operator||(task<Left> left, task<> right) {
    auto [l, r] = co_await wait_parallel_task_ready(parallel_task_type::wait_one_succ, std::move(left), std::move(right));
    const auto le = l.get_exception();
    const auto re = r.get_exception();

    if (l.get_index() == 0) {
        if (!le) {
            co_return std::variant<Left, std::monostate>{std::in_place_index<0>, std::move(l).result()};
        }

        if (!re) {
            co_return std::variant<Left, std::monostate>{std::in_place_index<1>};
        }

        throw multiple_exceptions(le);
    }

    if (!re) {
        co_return std::variant<Left, std::monostate>{std::in_place_index<1>};
    }

    if (!le) {
        co_return std::variant<Left, std::monostate>{std::in_place_index<0>, std::move(l).result()};
    }

    throw multiple_exceptions(re);
}

template <typename Right>
task<std::variant<std::monostate, Right>> operator||(task<> left, task<Right> right) {
    auto [l, r] = co_await wait_parallel_task_ready(parallel_task_type::wait_one_succ, std::move(left), std::move(right));
    const auto le = l.get_exception();
    const auto re = r.get_exception();

    if (l.get_index() == 0) {
        if (!le) {
            co_return std::variant<std::monostate, Right>{std::in_place_index<0>};
        }

        if (!re) {
            co_return std::variant<std::monostate, Right>{std::in_place_index<1>, std::move(r).result()};
        }

        throw multiple_exceptions(le);
    }

    if (!re) {
        co_return std::variant<std::monostate, Right>{std::in_place_index<1>, std::move(r).result()};
    }

    if (!le) {
        co_return std::variant<std::monostate, Right>{std::in_place_index<0>};
    }

    throw multiple_exceptions(re);
}

template <typename Left, typename Right>
task<std::variant<Left, Right>> operator||(task<Left> left, task<Right> right) {
    auto [l, r] = co_await wait_parallel_task_ready(parallel_task_type::wait_one_succ, std::move(left), std::move(right));
    const auto le = l.get_exception();
    const auto re = r.get_exception();

    if (l.get_index() == 0) {
        if (!le) {
            co_return std::variant<Left, Right>{std::in_place_index<0>, std::move(l).result()};
        }

        if (!re) {
            co_return std::variant<Left, Right>{std::in_place_index<1>, std::move(r).result()};
        }

        throw multiple_exceptions(le);
    }

    if (!re) {
        co_return std::variant<Left, Right>{std::in_place_index<1>, std::move(r).result()};
    }

    if (!le) {
        co_return std::variant<Left, Right>{std::in_place_index<0>, std::move(l).result()};
    }

    throw multiple_exceptions(re);
}

template <typename... Left>
task<std::variant<Left..., std::monostate>> operator||(task<std::variant<Left...>> left, task<> right) {
    auto [l, r] = co_await wait_parallel_task_ready(parallel_task_type::wait_one_succ, std::move(left), std::move(right));
    const auto le = l.get_exception();
    const auto re = r.get_exception();
    using widen = widen_variant<Left..., std::monostate>;

    if (l.get_index() == 0) {
        if (!le) {
            co_return widen::template call<0>(l.result());
        }

        if (!re) {
            co_return std::variant<Left..., std::monostate>{std::in_place_index<sizeof...(Left)>};
        }

        throw multiple_exceptions(le);
    }

    if (!re) {
        co_return std::variant<Left..., std::monostate>{std::in_place_index<sizeof...(Left)>};
    }

    if (!le) {
        co_return widen::template call<0>(l.result());
    }

    throw multiple_exceptions(re);
}

template <typename... Left, typename Right>
task<std::variant<Left..., Right>> operator||(task<std::variant<Left...>> left, task<Right> right) {
    auto [l, r] = co_await wait_parallel_task_ready(parallel_task_type::wait_one_succ, std::move(left), std::move(right));
    const auto le = l.get_exception();
    const auto re = r.get_exception();
    using widen = widen_variant<Left..., Right>;

    if (l.get_index() == 0) {
        if (!le) {
            co_return widen::template call<0>(l.result());
        }

        if (!re) {
            co_return std::variant<Left..., Right>{std::in_place_index<sizeof...(Left)>, std::move(r).result()};
        }

        throw multiple_exceptions(le);
    }

    if (!re) {
        co_return std::variant<Left..., Right>{std::in_place_index<sizeof...(Left)>, std::move(r).result()};
    }

    if (!le) {
        co_return widen::template call<0>(l.result());
    }

    throw multiple_exceptions(re);
}

}  // namespace simple
