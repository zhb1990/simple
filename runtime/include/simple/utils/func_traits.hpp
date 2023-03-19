#pragma once
#include <functional>
#include <simple/utils/type_traits.hpp>
#include <tuple>
#include <vector>

namespace simple {

class bad_params final : public std::exception {
  public:
    [[nodiscard]] const char* what() const noexcept override { return "params num is invalid"; }
};

template <typename T>
struct call_traits;

template <typename Ret, typename... Args>
struct call_traits<Ret(Args...)> : std::true_type {
    static constexpr size_t arity = sizeof...(Args);
    using pointer_type = std::add_pointer_t<Ret(Args...)>;
    using function_type = std::remove_pointer_t<pointer_type>;
    using std_function_type = std::function<Ret(Args...)>;
    using return_type = Ret;
    using args_tuple = std::tuple<Args...>;
    using args_decay_tuple = std::tuple<std::decay_t<Args>...>;

    static args_tuple forward_as_tuple(std::vector<param>& params) {
        if (arity != params.size()) {
            throw bad_params{};
        }

        auto forward = [&params]<std::size_t... Is>(std::index_sequence<Is...>) -> args_tuple {
            return std::forward_as_tuple(params[Is].template cast<Args>()...);
        };

        return forward(std::make_index_sequence<arity>{});
    }
};

template <typename Ret, typename... Args>
struct call_traits<Ret (*)(Args...)> : call_traits<Ret(Args...)> {};

template <typename Ret, typename... Args>
struct call_traits<std::function<Ret(Args...)>> : call_traits<Ret(Args...)> {};

template <typename ReturnType, typename ClassType, typename... Args>
struct call_traits<ReturnType (ClassType::*)(Args...)> : call_traits<ReturnType(Args...)> {};

template <typename ReturnType, typename ClassType, typename... Args>
struct call_traits<ReturnType (ClassType::*)(Args...) const> : call_traits<ReturnType(Args...)> {};

template <typename Callable>
struct call_traits : call_traits<decltype(&Callable::operator())> {};

}  // namespace simple
