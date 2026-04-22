// reflect/visit.hpp — Generic struct field iteration via C++26 reflection
//
// The building block for print, compare, hash, JSON, and more.
//
// Usage:
//   struct Point { int x; double y; };
//   Point p{3, 4.5};
//
//   // Visit every field with a lambda(name, value):
//   reflect::for_each_field(p, [](std::string_view name, auto const& value) {
//       std::println("  {} = {}", name, value);
//   });
//   // Output:
//   //   x = 3
//   //   y = 4.5
//
//   // Visit with index(index, name, value):
//   reflect::for_each_field_indexed(p, [](auto idx, std::string_view name, auto const& val) {
//       // idx is a std::integral_constant — usable in constexpr context
//   });
//
//   // Get the number of reflected fields:
//   reflect::field_count<Point>()  → 2
//
//   // Get field names:
//   reflect::field_names<Point>()  → {"x", "y"}
//
// SPDX-License-Identifier: MIT

#ifndef REFLECT_VISIT_HPP
#define REFLECT_VISIT_HPP

#include "reflect/traits.hpp"
#include <array>
#include <cstddef>
#include <string_view>
#include <tuple>
#include <utility>

namespace reflect {

namespace detail {
    // P2996R10 requires an access_context for member queries. We use
    // `current()` so the library sees whatever the caller's scope sees —
    // public members for typical users, more if the caller has friend
    // access.
    consteval auto members_of(std::meta::info t) {
        return std::meta::nonstatic_data_members_of(
            t, std::meta::access_context::current());
    }
} // namespace detail

// ---------------------------------------------------------------------------
// field_count<T>() — number of non-static data members
// ---------------------------------------------------------------------------

template <reflectable T>
consteval std::size_t field_count() {
    return detail::members_of(^^T).size();
}

// ---------------------------------------------------------------------------
// field_names<T>() — array of field name strings
// ---------------------------------------------------------------------------

template <reflectable T>
consteval auto field_names() {
    constexpr auto count = field_count<T>();
    std::array<std::string_view, count> names{};
    std::size_t i = 0;
    template for (constexpr auto member : detail::members_of(^^T)) {
        names[i++] = std::meta::identifier_of(member);
    }
    return names;
}

// ---------------------------------------------------------------------------
// for_each_field(obj, visitor) — call visitor(name, value) for every field
//
// The visitor signature should be:
//   void visitor(std::string_view name, auto const& value)
// or:
//   void visitor(std::string_view name, auto& value)  (for mutation)
// ---------------------------------------------------------------------------

template <reflectable T, typename F>
constexpr void for_each_field(T& obj, F&& visitor) {
    template for (constexpr auto member : detail::members_of(^^T)) {
        visitor(
            std::meta::identifier_of(member),
            obj.[:member:]
        );
    }
}

template <reflectable T, typename F>
constexpr void for_each_field(T const& obj, F&& visitor) {
    template for (constexpr auto member : detail::members_of(^^T)) {
        visitor(
            std::meta::identifier_of(member),
            obj.[:member:]
        );
    }
}

// ---------------------------------------------------------------------------
// for_each_field_indexed(obj, visitor) — visitor(index, name, value)
//
// index is std::integral_constant<std::size_t, I> so it's constexpr-usable.
// ---------------------------------------------------------------------------

namespace detail {
    template <std::size_t I>
    using index_c = std::integral_constant<std::size_t, I>;
}

template <reflectable T, typename F>
constexpr void for_each_field_indexed(T const& obj, F&& visitor) {
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        constexpr auto members = detail::members_of(^^T);
        (visitor(
            detail::index_c<Is>{},
            std::meta::identifier_of(members[Is]),
            obj.[:members[Is]:]
        ), ...);
    }(std::make_index_sequence<field_count<T>()>{});
}

template <reflectable T, typename F>
constexpr void for_each_field_indexed(T& obj, F&& visitor) {
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        constexpr auto members = detail::members_of(^^T);
        (visitor(
            detail::index_c<Is>{},
            std::meta::identifier_of(members[Is]),
            obj.[:members[Is]:]
        ), ...);
    }(std::make_index_sequence<field_count<T>()>{});
}

// ---------------------------------------------------------------------------
// for_each_field_meta(visitor) — visitor(meta::info) for metaprogramming
//
// Iterates field reflections without an object instance.
// Useful for generating code, building schemas, etc.
// ---------------------------------------------------------------------------

template <reflectable T, typename F>
consteval void for_each_field_meta(F&& visitor) {
    template for (constexpr auto member : detail::members_of(^^T)) {
        visitor(member);
    }
}

// ---------------------------------------------------------------------------
// field_value<Name>(obj) — access field by compile-time string
// ---------------------------------------------------------------------------

namespace detail {
    template <reflectable T>
    consteval std::meta::info find_field(std::string_view name) {
        for (auto member : members_of(^^T)) {
            if (std::meta::identifier_of(member) == name)
                return member;
        }
        // Reaching here means the field doesn't exist. Returning a null
        // reflection forces a compile error at the splice site.
        return std::meta::info{};
    }
}

// Usage: reflect::field_value<"x">(point) → point.x
template <auto Name, reflectable T>
constexpr auto& field_value(T& obj) {
    constexpr auto member = detail::find_field<T>(Name);
    static_assert(member != std::meta::info{}, "field not found");
    return obj.[:member:];
}

template <auto Name, reflectable T>
constexpr auto const& field_value(T const& obj) {
    constexpr auto member = detail::find_field<T>(Name);
    static_assert(member != std::meta::info{}, "field not found");
    return obj.[:member:];
}

// ---------------------------------------------------------------------------
// has_field<T>(name) — check if a type has a field with given name
// ---------------------------------------------------------------------------

template <reflectable T>
consteval bool has_field(std::string_view name) {
    for (auto member : detail::members_of(^^T)) {
        if (std::meta::identifier_of(member) == name)
            return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// transform_fields(obj, transformer) → tuple of mapped values
// ---------------------------------------------------------------------------

template <reflectable T, typename F>
constexpr auto transform_fields(T const& obj, F&& transformer) {
    return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        constexpr auto members = detail::members_of(^^T);
        return std::make_tuple(transformer(
            std::meta::identifier_of(members[Is]),
            obj.[:members[Is]:]
        )...);
    }(std::make_index_sequence<field_count<T>()>{});
}

} // namespace reflect

#endif // REFLECT_VISIT_HPP
