#pragma once

#include <any>
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

class bad_lvalue_cast final : public std::exception {
  public:
    const char* what() const noexcept override { return "lvalue reference cast fail"; }
};

constexpr uint32_t reference_attributes_none = 0;
constexpr uint32_t reference_attributes_const = 1;
constexpr uint32_t reference_attributes_point_const = 2;
constexpr uint32_t reference_attributes_array = 4;

struct lvalue_reference {
    void* value{nullptr};
    const std::type_info* raw{nullptr};
    const std::type_info* remove_point{nullptr};

    uint32_t attributes{reference_attributes_none};

    template <typename T>
    requires(!std::is_array_v<std::remove_cvref_t<T>> || (std::is_reference_v<T> && !std::is_rvalue_reference_v<T>))
    T cast() {
        if (typeid(T) == *raw) {
            if constexpr (!std::is_const_v<T>) {
                if (attributes & reference_attributes_const) {
                    throw bad_lvalue_cast{};
                }
            }

            return static_cast<T>(*static_cast<std::add_pointer_t<T>>(value));
        }

        using remove_cvref_t = std::remove_cvref_t<T>;
        if constexpr (std::is_pointer_v<remove_cvref_t>) {
            using remove_point_t = std::remove_pointer_t<remove_cvref_t>;
            if (!remove_point || *remove_point != typeid(remove_point_t)) {
                throw bad_lvalue_cast{};
            }

            if constexpr (!std::is_const_v<remove_point_t>) {
                if (attributes & reference_attributes_point_const) {
                    throw bad_lvalue_cast{};
                }
            }

            if (attributes & reference_attributes_array) {
                if constexpr (!std::is_reference_v<T>) {
                    return static_cast<T>(value);
                } else {
                    throw bad_lvalue_cast{};
                }
            }

            return static_cast<T>(*static_cast<std::add_pointer_t<T>>(value));
        } else {
            throw bad_lvalue_cast{};
        }
    }
};

template <typename T>
lvalue_reference make_reference(T& t) {
    using decay_t = std::decay_t<T>;
    lvalue_reference lr{.raw = &typeid(std::remove_cvref_t<T>)};

    if constexpr (std::is_const_v<std::remove_reference_t<T>>) {
        lr.attributes += reference_attributes_const;
        lr.value = const_cast<void*>(static_cast<const void*>(&t));
    } else {
        lr.value = &t;
    }

    if constexpr (std::is_pointer_v<decay_t>) {
        using remove_point_t = std::remove_pointer_t<decay_t>;
        lr.remove_point = &typeid(std::remove_cvref_t<remove_point_t>);
        if constexpr (std::is_const_v<remove_point_t>) {
            lr.attributes += reference_attributes_point_const;
        }

        if constexpr (std::is_array_v<std::remove_cvref_t<T>>) {
            lr.attributes += reference_attributes_array;
        }
    }

    return lr;
}

struct param {
    using value_t = std::variant<std::monostate, lvalue_reference, std::any>;
    value_t value;

    template <typename T>
    remove_rvalue_reference_t<T> cast() {
        if (std::holds_alternative<std::any>(value)) {
            if constexpr (std::is_rvalue_reference_v<T>) {
                return std::any_cast<T>(std::get<std::any>(std::move(value)));
            } else {
                return std::any_cast<T>(std::get<std::any>(value));
            }
        } else {
            return std::get<lvalue_reference>(value).template cast<T>();
        }
    }
};

template <typename T>
[[nodiscard]] param make_param(T&& t) {
    if constexpr (std::is_reference_v<T&&> && !std::is_rvalue_reference_v<T&&>) {
        return {param::value_t{std::in_place_index<1>, make_reference(t)}};
    } else {
        return {param::value_t{std::in_place_index<2>, std::any(t)}};
    }
}

}  // namespace simple
