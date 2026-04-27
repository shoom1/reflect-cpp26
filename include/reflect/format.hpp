// reflect/format.hpp — Automatic std::format / std::print support
//
// Makes any reflectable struct formattable with zero boilerplate.
//
// Usage:
//   struct Point { int x; int y; };
//
//   // Option 1: Use reflect::format() directly
//   std::string s = reflect::format(Point{3, 7});
//   // → "Point{.x=3, .y=7}"
//
//   // Option 2: Register for std::format (requires macro due to template rules)
//   REFLECT_MAKE_FORMATTABLE(Point);
//
//   std::format("{}", Point{3, 7});
//   // → "Point{.x=3, .y=7}"
//
//   // Format specifiers:
//   //   {} or {:d}  → debug/pretty:  Point{.x=3, .y=7}
//   //   {:j}        → JSON compact:  {"x":3,"y":7}
//   //   {:J}        → JSON pretty (newline-indented, see to_json's indent option)
//
// SPDX-License-Identifier: MIT

#ifndef REFLECT_FORMAT_HPP
#define REFLECT_FORMAT_HPP

#include "reflect/traits.hpp"
#include <format>
#include <string>
#include <string_view>

// Pull in print and json for format dispatch
#if __has_include("reflect/print.hpp")
#include "reflect/print.hpp"
#define REFLECT_FMT_HAS_PRINT 1
#else
#define REFLECT_FMT_HAS_PRINT 0
#endif

#if __has_include("reflect/json.hpp")
#include "reflect/json.hpp"
#define REFLECT_FMT_HAS_JSON 1
#else
#define REFLECT_FMT_HAS_JSON 0
#endif

namespace reflect {

namespace format_detail {

    template <typename T>
    concept is_reflectable = reflect::reflectable<T>;

    enum class format_mode { debug, json_compact, json_pretty };

    // Must be constexpr: it's called from the formatter's `parse`
    // method, which in turn is invoked by std::basic_format_string's
    // *consteval* constructor when validating the format string at
    // compile time. An `inline` (non-constexpr) function in that chain
    // breaks std::format("{:j}", obj) with: "call to consteval function
    // 'basic_format_string<…>' is not a constant expression".
    constexpr format_mode parse_mode(std::string_view spec) {
        for (char c : spec) {
            if (c == 'j') return format_mode::json_compact;
            if (c == 'J') return format_mode::json_pretty;
            if (c == 'd') return format_mode::debug;
        }
        return format_mode::debug;
    }

} // namespace format_detail

// ---------------------------------------------------------------------------
// reflect::format(value, mode) — format to string
// ---------------------------------------------------------------------------

template <typename T>
std::string format(T const& value, format_detail::format_mode mode = format_detail::format_mode::debug) {
    switch (mode) {
        case format_detail::format_mode::debug:
#if REFLECT_FMT_HAS_PRINT
            return reflect::to_string(value);
#else
            return "(reflect/print.hpp not included)";
#endif

        case format_detail::format_mode::json_compact:
#if REFLECT_FMT_HAS_JSON
            return reflect::to_json(value);
#else
            return "(reflect/json.hpp not included)";
#endif

        case format_detail::format_mode::json_pretty:
#if REFLECT_FMT_HAS_JSON
            return reflect::to_json(value, {.indent = 2});
#else
            return "(reflect/json.hpp not included)";
#endif
    }
    return "";
}

} // namespace reflect

// ---------------------------------------------------------------------------
// REFLECT_MAKE_FORMATTABLE(Type) — register a type with std::formatter
//
// Must be used at namespace scope (not inside a function).
// This macro is necessary because std::formatter specializations must be
// in namespace std, which can't be done generically with templates alone.
// ---------------------------------------------------------------------------

#define REFLECT_MAKE_FORMATTABLE(Type)                                         \
    template <>                                                                \
    struct std::formatter<Type> {                                              \
        ::reflect::format_detail::format_mode mode_ =                          \
            ::reflect::format_detail::format_mode::debug;                      \
                                                                               \
        constexpr auto parse(std::format_parse_context& ctx) {                 \
            auto it = ctx.begin();                                             \
            auto end = ctx.end();                                              \
            std::string_view spec;                                             \
            if (it != end && *it != '}') {                                     \
                auto start = it;                                               \
                while (it != end && *it != '}') ++it;                          \
                spec = std::string_view(&*start, static_cast<std::size_t>(     \
                    it - start));                                              \
                mode_ = ::reflect::format_detail::parse_mode(spec);            \
            }                                                                  \
            return it;                                                         \
        }                                                                      \
                                                                               \
        auto format(Type const& val, std::format_context& ctx) const {         \
            auto str = ::reflect::format(val, mode_);                          \
            return std::format_to(ctx.out(), "{}", str);                       \
        }                                                                      \
    }

// ---------------------------------------------------------------------------
// Convenience macro for multiple types at once
// ---------------------------------------------------------------------------

#define REFLECT_MAKE_FORMATTABLE_2(T1, T2)    \
    REFLECT_MAKE_FORMATTABLE(T1);             \
    REFLECT_MAKE_FORMATTABLE(T2)

#define REFLECT_MAKE_FORMATTABLE_3(T1, T2, T3)  \
    REFLECT_MAKE_FORMATTABLE(T1);               \
    REFLECT_MAKE_FORMATTABLE(T2);               \
    REFLECT_MAKE_FORMATTABLE(T3)

#define REFLECT_MAKE_FORMATTABLE_4(T1, T2, T3, T4)  \
    REFLECT_MAKE_FORMATTABLE(T1);                    \
    REFLECT_MAKE_FORMATTABLE(T2);                    \
    REFLECT_MAKE_FORMATTABLE(T3);                    \
    REFLECT_MAKE_FORMATTABLE(T4)

#endif // REFLECT_FORMAT_HPP
