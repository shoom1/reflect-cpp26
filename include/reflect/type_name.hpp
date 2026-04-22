// reflect/type_name.hpp — Clean, demangled type names via C++26 reflection
//
// Usage:
//   reflect::type_name<int>()              → "int"
//   reflect::type_name<std::vector<int>>() → "std::vector<int>"
//   reflect::type_name(my_variable)        → type name of the variable's type
//
// SPDX-License-Identifier: MIT

#ifndef REFLECT_TYPE_NAME_HPP
#define REFLECT_TYPE_NAME_HPP

#include "reflect/traits.hpp"
#include <string_view>
#include <string>

namespace reflect {

// ---------------------------------------------------------------------------
// type_name<T>() — returns a human-readable name for any type
// ---------------------------------------------------------------------------

template <typename T>
consteval std::string_view type_name() {
    return std::meta::display_string_of(^^T);
}

// ---------------------------------------------------------------------------
// type_name(auto const&) — deduces the type from a value
// ---------------------------------------------------------------------------

template <typename T>
consteval std::string_view type_name(T const&) {
    return type_name<T>();
}

// ---------------------------------------------------------------------------
// qualified_type_name<T>() — includes cv-qualifiers and ref-ness
// ---------------------------------------------------------------------------

template <typename T>
consteval std::string_view qualified_type_name() {
    return std::meta::display_string_of(^^T);
}

// ---------------------------------------------------------------------------
// identifier<^^entity>() — returns the declared name of any reflected entity
// ---------------------------------------------------------------------------

template <std::meta::info R>
consteval std::string_view identifier() {
    return std::meta::identifier_of(R);
}

} // namespace reflect

#endif // REFLECT_TYPE_NAME_HPP
