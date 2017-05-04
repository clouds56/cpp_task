//
// Created by Clouds Flowing on 5/4/17.
//

#ifndef CPP_TASK_COMMON_H
#define CPP_TASK_COMMON_H

#include <type_traits>
#include <functional>

namespace std_exp {

template <typename T>
struct optional {
    // std::optional is c++17, this is a simple version (untested)
    T v;
    bool b;
    constexpr optional() : b(false) { }
    constexpr optional(const T &v) : v(v), b(true) { }
    constexpr optional(T &&v) : v(std::move(v)), b(true) { }
    constexpr optional(const optional& o) : b(o.b), v(o.v) { }
    constexpr optional(optional&& o) : b(o.b), v(std::move(o.v)) { }

    optional& operator=(const T& v) { v = v; b = true; return *this; }
    optional& operator=(T&& v) { v = std::move(v); b = true; return *this; }
    optional& operator=(const optional& o) { b = o.b, v = o.v; return *this; }
    optional& operator=(optional&& o) { b = o.b, v = std::move(o.v); return *this; }

    constexpr T& operator*() & { return v; }
    constexpr const T& operator*() const& { return v; }
    constexpr T&& operator*() && { return v; }
    constexpr const T&& operator*() const&& { return v; }

    constexpr bool hasValue() const { return b; }
};

template <class F, class... Args>
static auto __invoke(F &&f, Args&&... args) ->
decltype(std::forward<F>(f)(std::forward<Args>(args)...)) {
    return std::forward<F>(f)(std::forward<Args>(args)...);
}

template <class F, class... Args>
struct invoke_result {
//    typedef decltype(__invoke(std::declval<F>(), std::declval<Args>()...)) type;
    typedef decltype(std::declval<F>()(std::declval<Args>()...)) type;
};

template <class T>
std::decay_t<T> decay_copy(T&& v) { return std::forward<T>(v); }

template <class F, class Tuple, std::size_t... I>
constexpr decltype(auto) __apply_impl(F &&f, Tuple &&t, std::index_sequence<I...>)
{
    return std_exp::__invoke(std::forward<F>(f), std::get<I>(std::forward<Tuple>(t))...);
}
template <class F, class Tuple>
constexpr decltype(auto) apply(F &&f, Tuple &&t)
{
    return __apply_impl(
            std::forward<F>(f), std::forward<Tuple>(t),
            std::make_index_sequence<std::tuple_size<std::decay_t<Tuple>>::value>{});
}

}

#endif //CPP_TASK_COMMON_H
