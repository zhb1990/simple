#pragma once

#include <system_error>
#include <type_traits>
#include <variant>

namespace simple {

template <typename T>
concept array_type = std::is_array_v<std::remove_cvref_t<T>>;

template <template <typename...> class U, typename T>
struct is_template_instant_of : std::false_type {};

template <template <typename...> class U, typename... Args>
struct is_template_instant_of<U, U<Args...>> : std::true_type {};

template <template <typename...> class U, typename T>
concept template_instant_of = is_template_instant_of<U, std::remove_cvref_t<T>>::value;

template <typename T>
concept emplace_back = requires(T t) {
                           typename T::value_type;
                           t.emplace_back(typename T::value_type{});
                       };

template <typename T, typename Compare>
concept compare_able = std::is_invocable_r_v<bool, Compare, const T&, const T&>;

template <typename T>
struct class_traits;

template <typename Class, typename Field>
struct class_traits<Field Class::*> {
    using class_type = Class;
    using field_type = Field;

    static class_type* from_field(field_type* ptr, Field Class::*field) noexcept {
        if (!ptr) {
            return nullptr;
        }

        return reinterpret_cast<class_type*>(
            reinterpret_cast<char*>(ptr) -
            reinterpret_cast<size_t>(&reinterpret_cast<char const volatile&>((reinterpret_cast<class_type*>(0)->*field))));
    }

    static const class_type* from_field(const field_type* ptr, Field Class::*field) noexcept {
        if (!ptr) {
            return nullptr;
        }

        return reinterpret_cast<const class_type*>(reinterpret_cast<const char*>(ptr) -
                                                   reinterpret_cast<size_t>(&reinterpret_cast<char const volatile&>(
                                                       (reinterpret_cast<const class_type*>(0)->*field))));
    }
};

template <typename T>
using class_type = typename class_traits<T>::class_type;

template <typename T>
using field_type = typename class_traits<T>::field_type;

template <typename T>
concept class_field_type = requires {
                               typename class_traits<T>::class_type;
                               typename class_traits<T>::field_type;
                           };

template <typename Container>
concept is_container = requires(Container container) {
                           typename Container::value_type;
                           container.begin();
                           container.end();
                           container.size();
                       };

template <typename T>
struct remove_rvalue_reference {
    using type = T;
};

template <typename T>
struct remove_rvalue_reference<T&&> {
    using type = T;
};

template <typename T>
using remove_rvalue_reference_t = typename remove_rvalue_reference<T>::type;

template <typename... T>
struct widen_variant {
    template <std::size_t I, typename SourceVariant>
    static std::variant<T...> call(SourceVariant& source) {
        if (source.index() == I) {
            return std::variant<T...>{std::in_place_index<I>, std::move(std::get<I>(source))};
        }

        if constexpr (I + 1 < std::variant_size_v<SourceVariant>) {
            return call<I + 1>(source);
        }

        throw std::logic_error("empty variant");
    }
};

template <typename T>
struct is_normal_func_help : std::false_type {};

template <typename Ret, typename... Args>
struct is_normal_func_help<Ret(Args...)> : std::true_type {};

template <typename Ret, typename... Args>
struct is_normal_func_help<Ret (*)(Args...)> : std::true_type {};

template <typename Func>
concept is_normal_func = is_normal_func_help<Func>::value;

}  // namespace simple
