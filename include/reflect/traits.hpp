// reflect/traits.hpp — Shared concepts and meta include for reflect library
//
// Centralizes the C++26 reflection header include and common type traits
// used across multiple reflect modules.
//
// SPDX-License-Identifier: MIT

#ifndef REFLECT_TRAITS_HPP
#define REFLECT_TRAITS_HPP

#if __has_include(<meta>)
#include <meta>
#elif __has_include(<experimental/meta>)
#include <experimental/meta>
#else
#error "C++26 reflection support required"
#endif

#include <concepts>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>

namespace reflect {

// ---------------------------------------------------------------------------
// Helper concepts — categorize "encapsulated" stdlib types so downstream
// modules can route them to native-API handlers instead of treating them
// as reflectable structs.
// ---------------------------------------------------------------------------

template <typename T>
concept is_string_like = std::convertible_to<T, std::string_view> ||
                         std::same_as<std::remove_cvref_t<T>, std::string>;

template <typename T>
concept is_optional = requires(T t) {
    typename std::remove_cvref_t<T>::value_type;
    { t.has_value() } -> std::convertible_to<bool>;
    { *t };
} && !is_string_like<T>;

template <typename T>
concept is_range = std::ranges::range<T> && !is_string_like<T>;

// ---------------------------------------------------------------------------
// `reflectable<T>` — T is a "data class" whose logical state is its
// public non-static data members.
//
// We require:
//   * T is a class type;
//   * T's members are queryable via P2996 reflection;
//   * T has no private/protected non-static data members (implementation
//     is fully exposed — the members ARE the logical state);
//   * T is not one of the encapsulated standard-library categories
//     (string, optional, range). Those are handled via their public APIs
//     in print/json/compare/etc., not by member-by-member reflection.
//
// Stdlib types like std::string, std::vector, std::filesystem::path,
// std::chrono::time_point, std::function fail this test (they hide state).
// User aggregates and PODs typically pass.
// ---------------------------------------------------------------------------

namespace detail {

template <typename T>
consteval bool all_data_members_public() {
    auto visible = std::meta::nonstatic_data_members_of(
        ^^T, std::meta::access_context::unprivileged());
    auto all = std::meta::nonstatic_data_members_of(
        ^^T, std::meta::access_context::unchecked());
    return visible.size() == all.size();
}

} // namespace detail

template <typename T>
concept reflectable = std::is_class_v<T>
    && !is_string_like<T>
    && !is_optional<T>
    && !is_range<T>
    && requires {
        std::meta::nonstatic_data_members_of(
            ^^T, std::meta::access_context::unchecked());
    }
    && detail::all_data_members_public<T>();

} // namespace reflect

#endif // REFLECT_TRAITS_HPP
