#pragma once

#include <simple/config.h>

#include <cstdint>
#include <simple/utils/func_traits.hpp>
#include <string>
#include <string_view>
#include <unordered_map>

namespace simple {

class call_exception final : public std::exception {
  public:
    explicit call_exception(std::string_view name) {
        error_.append("function '");
        error_.append(name);
        error_.append("' not found");
    }

    [[nodiscard]] const char* what() const noexcept override { return error_.c_str(); }

  private:
    std::string error_;
};

class call_router {
  public:
    template <typename Func>
    void register_call(std::string_view name, Func func) {
        call_map_[name] = [fn = std::forward<Func>(func)](std::vector<param>& args) -> param {
            using traits = call_traits<Func>;
            using return_t = typename traits::return_type;

            if constexpr (traits::arity == 0) {
                if constexpr (std::same_as<return_t, void>) {
                    fn();
                    return {};
                } else {
                    return make_param(fn());
                }
            } else {
                if constexpr (std::same_as<return_t, void>) {
                    std::apply(fn, traits::forward_as_tuple(args));
                    return {};
                } else {
                    return make_param(std::apply(fn, traits::forward_as_tuple(args)));
                }
            }
        };
    }

    template <typename Class, typename Func>
    void register_call(std::string_view name, Func Class::*func, Class* self) {
        call_map_[name] = [func, self](std::vector<param>& args) -> param {
            using traits = call_traits<Func Class::*>;
            using return_t = typename traits::return_type;
            using args_t = typename traits::args_tuple;

            if constexpr (traits::arity == 0) {
                if constexpr (std::same_as<return_t, void>) {
                    (self->*func)();
                    return {};
                } else {
                    return make_param((self->*func)());
                }
            } else {
                auto apply = [&]<std::size_t... Is>(args_t&& t, std::index_sequence<Is...>) -> return_t {
                    return (self->*func)(std::get<Is>(std::forward<args_t>(t))...);
                };
                if constexpr (std::same_as<return_t, void>) {
                    apply(traits::forward_as_tuple(args), std::make_index_sequence<traits::arity>{});
                    return {};
                } else {
                    return make_param(apply(traits::forward_as_tuple(args), std::make_index_sequence<traits::arity>{}));
                }
            }
        };
    }

    template <typename ReturnType = void, typename... Args>
    ReturnType call(std::string_view name, Args&&... args) {
        const auto it = call_map_.find(name);
        if (it == call_map_.end()) {
            throw call_exception(name);
        }

        std::vector<param> params;
        if constexpr (sizeof...(args) > 0) {
            params = std::vector{make_param(args)...};
        }
        if constexpr (std::same_as<ReturnType, void>) {
            it->second(params);
        } else {
            return it->second(params).template cast<ReturnType>();
        }
    }

    bool deregister_call(std::string_view name) { return call_map_.erase(name) > 0; }

  private:
    using func_wrapper = std::function<param(std::vector<param>&)>;
    std::unordered_map<std::string_view, func_wrapper> call_map_;
};

}  // namespace simple
