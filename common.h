//
// Created by Clouds Flowing on 5/4/17.
//

#ifndef CPP_TASK_COMMON_H
#define CPP_TASK_COMMON_H

#include <type_traits>

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

}

#endif //CPP_TASK_COMMON_H
