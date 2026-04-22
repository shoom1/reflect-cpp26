// reflect/tuple.hpp — Struct ↔ tuple conversion via C++26 reflection
//
// Replaces Boost.PFR's aggregate-to-tuple with zero hacks.
//
// Usage:
//   struct Point { int x; double y; };
//   Point p{3, 4.5};
//
//   auto t = reflect::to_tuple(p);           // std::tuple<int, double>{3, 4.5}
//   auto p2 = reflect::from_tuple<Point>(t); // Point{3, 4.5}
//
//   // Apply a function to all fields:
//   reflect::apply(p, [](int x, double y) { return x + y; }); // → 7.5
//
//   // Structured-binding-like access by index:
//   reflect::get<0>(p) → 3
//   reflect::get<1>(p) → 4.5
//
// SPDX-License-Identifier: MIT

#ifndef REFLECT_TUPLE_HPP
#define REFLECT_TUPLE_HPP

#include "reflect/traits.hpp"
#include <cstddef>
#include <functional>
#include <tuple>
#include <utility>

namespace reflect {

namespace detail {
    consteval auto tuple_members_of(std::meta::info t) {
        return std::meta::nonstatic_data_members_of(
            t, std::meta::access_context::current());
    }

    // Build a tuple type from a struct's member types
    template <reflectable T>
    consteval auto make_tuple_type() {
        return []<std::size_t... Is>(std::index_sequence<Is...>) {
            constexpr auto members = tuple_members_of(^^T);
            return ^^std::tuple<typename[:std::meta::type_of(members[Is]):]...>;
        }(std::make_index_sequence<tuple_members_of(^^T).size()>{});
    }
} // namespace detail

// ---------------------------------------------------------------------------
// tuple_type<T> — the std::tuple type corresponding to T's fields
// ---------------------------------------------------------------------------

template <reflectable T>
using tuple_type = typename[:detail::make_tuple_type<T>():];

// ---------------------------------------------------------------------------
// to_tuple(obj) — convert struct to tuple of field values
// ---------------------------------------------------------------------------

template <reflectable T>
constexpr auto to_tuple(T const& obj) {
    return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        constexpr auto members = detail::tuple_members_of(^^T);
        return std::make_tuple(obj.[:members[Is]:]...);
    }(std::make_index_sequence<detail::tuple_members_of(^^T).size()>{});
}

// Mutable version — returns tuple of references
template <reflectable T>
constexpr auto to_ref_tuple(T& obj) {
    return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        constexpr auto members = detail::tuple_members_of(^^T);
        return std::tie(obj.[:members[Is]:]...);
    }(std::make_index_sequence<detail::tuple_members_of(^^T).size()>{});
}

// ---------------------------------------------------------------------------
// from_tuple<T>(tuple) — construct a struct from a tuple
// ---------------------------------------------------------------------------

template <reflectable T, typename Tuple>
constexpr T from_tuple(Tuple const& t) {
    return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        return T{std::get<Is>(t)...};
    }(std::make_index_sequence<std::tuple_size_v<std::remove_cvref_t<Tuple>>>{});
}

// ---------------------------------------------------------------------------
// get<I>(obj) — access field by compile-time index
// ---------------------------------------------------------------------------

template <std::size_t I, reflectable T>
constexpr auto& get(T& obj) {
    constexpr auto members = detail::tuple_members_of(^^T);
    static_assert(I < members.size(), "Field index out of range");
    return obj.[:members[I]:];
}

template <std::size_t I, reflectable T>
constexpr auto const& get(T const& obj) {
    constexpr auto members = detail::tuple_members_of(^^T);
    static_assert(I < members.size(), "Field index out of range");
    return obj.[:members[I]:];
}

// ---------------------------------------------------------------------------
// apply(obj, f) — call f with all fields as arguments
//
//   reflect::apply(point, [](int x, double y) { return x + y; });
// ---------------------------------------------------------------------------

template <reflectable T, typename F>
constexpr decltype(auto) apply(T const& obj, F&& f) {
    return [&]<std::size_t... Is>(std::index_sequence<Is...>) -> decltype(auto) {
        constexpr auto members = detail::tuple_members_of(^^T);
        return std::invoke(std::forward<F>(f), obj.[:members[Is]:]...);
    }(std::make_index_sequence<detail::tuple_members_of(^^T).size()>{});
}

template <reflectable T, typename F>
constexpr decltype(auto) apply(T& obj, F&& f) {
    return [&]<std::size_t... Is>(std::index_sequence<Is...>) -> decltype(auto) {
        constexpr auto members = detail::tuple_members_of(^^T);
        return std::invoke(std::forward<F>(f), obj.[:members[Is]:]...);
    }(std::make_index_sequence<detail::tuple_members_of(^^T).size()>{});
}

// ---------------------------------------------------------------------------
// struct_to_struct<Target>(source) — convert between compatible structs
//
// Copies fields by index. Source and Target must have the same number of
// fields with compatible types.
// ---------------------------------------------------------------------------

template <reflectable Target, reflectable Source>
constexpr Target struct_to_struct(Source const& src) {
    constexpr auto src_members = detail::tuple_members_of(^^Source);
    constexpr auto tgt_members = detail::tuple_members_of(^^Target);
    static_assert(src_members.size() == tgt_members.size(),
                  "Source and target must have the same number of fields");

    return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        return Target{static_cast<typename[:std::meta::type_of(tgt_members[Is]):]>(
            src.[:src_members[Is]:]
        )...};
    }(std::make_index_sequence<src_members.size()>{});
}

} // namespace reflect

#endif // REFLECT_TUPLE_HPP
