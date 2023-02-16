#pragma once
#include <functional>

namespace simple {
template <typename T>
struct call_traits;

template <typename Ret, typename... Args>
struct call_traits<Ret(Args...)> : std::true_type {
    static constexpr size_t arity = sizeof...(Args);
    using pointer_type = std::add_pointer_t<Ret(Args...)>;
    using function_type = std::remove_pointer_t<pointer_type>;
    using std_function_type = std::function<function_type>;
    using return_type = Ret;
    using args_tuple = std::tuple<std::decay_t<Args>...>;
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
