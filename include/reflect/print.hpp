// reflect/print.hpp — Pretty-print any struct via C++26 reflection
//
// The Rust #[derive(Debug)] equivalent for C++.
//
// Usage:
//   struct Point { int x; double y; };
//   struct Line  { Point start; Point end; std::string label; };
//
//   Point p{3, 7};
//   Line  l{{0,0}, {10,20}, "diagonal"};
//
//   reflect::to_string(p) → R"(Point{.x=3, .y=7})"
//   reflect::to_string(l) → R"(Line{.start=Point{.x=0, .y=0}, .end=Point{.x=10, .y=20}, .label="diagonal"})"
//
//   // Print directly to ostream:
//   reflect::print(std::cout, p);
//   reflect::println(std::cout, l);
//
// Handles: nested structs, strings, containers, optionals, pointers, enums
// (enums require reflect/enum.hpp for named output, otherwise prints underlying value)
//
// SPDX-License-Identifier: MIT

#ifndef REFLECT_PRINT_HPP
#define REFLECT_PRINT_HPP

#include "reflect/traits.hpp"
#include <charconv>
#include <cstddef>
#include <iostream>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>

// Optional integration: if enum.hpp is included, we get named enum printing.
#if __has_include("reflect/enum.hpp")
#include "reflect/enum.hpp"
#define REFLECT_HAS_ENUM_HPP 1
#else
#define REFLECT_HAS_ENUM_HPP 0
#endif

namespace reflect {

// Forward declaration
template <typename T>
std::string to_string(T const& value);

namespace detail {

    consteval auto print_members_of(std::meta::info t) {
        return std::define_static_array(
            std::meta::nonstatic_data_members_of(
                t, std::meta::access_context::current()));
    }

    // ---------------------------------------------------------------------------
    // Type traits for dispatch
    // ---------------------------------------------------------------------------

    template <typename T>
    concept is_pair = requires(T t) {
        { t.first };
        { t.second };
    } && !reflectable<T> && !is_range<T> && !is_string_like<T>;

    template <typename T>
    concept is_smart_pointer = requires(T t) {
        typename T::element_type;
        { t.get() } -> std::convertible_to<void const*>;
        { *t };
        { static_cast<bool>(t) };
    } && !is_optional<T>;

    // ---------------------------------------------------------------------------
    // Core serialization dispatch — writes directly to std::string
    // ---------------------------------------------------------------------------

    template <typename T>
    void write_value(std::string& out, T const& value);

    // Booleans
    inline void write_value(std::string& out, bool value) {
        out += value ? "true" : "false";
    }

    // Arithmetic types (but not bool, not char) — uses to_chars
    template <typename T>
        requires (std::is_arithmetic_v<T> && !std::is_same_v<T, bool> && !std::is_same_v<T, char>)
    void write_value(std::string& out, T const& value) {
        char buf[64];
        auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), value);
        out.append(buf, static_cast<std::size_t>(ptr - buf));
    }

    // char
    inline void write_value(std::string& out, char value) {
        out += '\'';
        out += value;
        out += '\'';
    }

    // Strings — quoted
    template <reflect::is_string_like T>
    void write_value(std::string& out, T const& value) {
        out += '"';
        out += std::string_view(value);
        out += '"';
    }

    // C-string literal
    inline void write_value(std::string& out, char const* value) {
        if (value) {
            out += '"';
            out += value;
            out += '"';
        } else {
            out += "nullptr";
        }
    }

    // Enums
    template <typename T>
        requires std::is_enum_v<T>
    void write_value(std::string& out, T const& value) {
#if REFLECT_HAS_ENUM_HPP
        auto name = reflect::enum_to_string(value);
        if (!name.empty()) {
            out += std::meta::display_string_of(^^T);
            out += "::";
            out += name;
        } else {
            out += std::meta::display_string_of(^^T);
            out += '(';
            char buf[64];
            auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf),
                static_cast<std::underlying_type_t<T>>(value));
            out.append(buf, static_cast<std::size_t>(ptr - buf));
            out += ')';
        }
#else
        char buf[64];
        auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf),
            static_cast<std::underlying_type_t<T>>(value));
        out.append(buf, static_cast<std::size_t>(ptr - buf));
#endif
    }

    // nullptr
    inline void write_value(std::string& out, std::nullptr_t) {
        out += "nullptr";
    }

    // Raw pointers
    template <typename T>
        requires std::is_pointer_v<T>
    void write_value(std::string& out, T const& value) {
        if (value) {
            out += '&';
            write_value(out, *value);
        } else {
            out += "nullptr";
        }
    }

    // Smart pointers
    template <is_smart_pointer T>
    void write_value(std::string& out, T const& value) {
        if (value) {
            out += '&';
            write_value(out, *value);
        } else {
            out += "nullptr";
        }
    }

    // std::optional and optional-like
    template <reflect::is_optional T>
    void write_value(std::string& out, T const& value) {
        if (value.has_value()) {
            write_value(out, *value);
        } else {
            out += "nullopt";
        }
    }

    // Pairs
    template <is_pair T>
    void write_value(std::string& out, T const& value) {
        out += '(';
        write_value(out, value.first);
        out += ", ";
        write_value(out, value.second);
        out += ')';
    }

    // Ranges (vectors, arrays, sets, maps, etc.)
    template <reflect::is_range T>
    void write_value(std::string& out, T const& range) {
        out += '[';
        bool first = true;
        for (auto const& elem : range) {
            if (!first) out += ", ";
            first = false;
            write_value(out, elem);
        }
        out += ']';
    }

    // Reflectable structs — the main event
    template <reflectable T>
    void write_value(std::string& out, T const& obj) {
        out += std::meta::display_string_of(^^T);
        out += '{';
        bool first = true;
        template for (constexpr auto member : print_members_of(^^T)) {
            if (!first) out += ", ";
            first = false;
            out += '.';
            out += std::meta::identifier_of(member);
            out += '=';
            write_value(out, obj.[:member:]);
        }
        out += '}';
    }

} // namespace detail

// ---------------------------------------------------------------------------
// to_string(value) — universal pretty-print to string
// ---------------------------------------------------------------------------

template <typename T>
std::string to_string(T const& value) {
    std::string out;
    detail::write_value(out, value);
    return out;
}

// ---------------------------------------------------------------------------
// print / println — write directly to an ostream
// ---------------------------------------------------------------------------

template <typename T>
void print(std::ostream& os, T const& value) {
    os << to_string(value);
}

template <typename T>
void println(std::ostream& os, T const& value) {
    print(os, value);
    os << '\n';
}

// Convenience: print to stdout
template <typename T>
void print(T const& value) {
    print(std::cout, value);
}

template <typename T>
void println(T const& value) {
    println(std::cout, value);
}

// ---------------------------------------------------------------------------
// REFLECT_DEBUG(expr) — print "expr = value" using the macro stringification
// ---------------------------------------------------------------------------

#define REFLECT_DEBUG(expr) \
    (::reflect::print(std::cout, #expr " = "), ::reflect::println(std::cout, (expr)))

} // namespace reflect

#endif // REFLECT_PRINT_HPP
