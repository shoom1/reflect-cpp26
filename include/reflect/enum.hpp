// reflect/enum.hpp — Complete enum reflection via C++26
//
// Replaces magic_enum with zero hacks, zero compiler-specific tricks.
//
// Usage:
//   enum class Color { red, green, blue };
//
//   reflect::enum_to_string(Color::red)      → "red"
//   reflect::enum_from_string<Color>("green") → Color::green
//   reflect::enum_count<Color>()              → 3
//   reflect::enum_names<Color>()              → {"red", "green", "blue"}
//   reflect::enum_values<Color>()             → {Color::red, Color::green, Color::blue}
//   reflect::enum_contains<Color>(42)         → false
//
// Flags support:
//   enum class Perm { read = 1, write = 2, exec = 4 };
//   reflect::flags_to_string<Perm>(Perm::read | Perm::write) → "read|write"
//
// SPDX-License-Identifier: MIT

#ifndef REFLECT_ENUM_HPP
#define REFLECT_ENUM_HPP

#include "reflect/traits.hpp"
#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace reflect {

namespace enum_detail {

    // Wrap std::meta::enumerators_of in a local consteval helper so that
    // `template for (constexpr auto e : enum_members_of(^^E))` is a
    // constant expression in `constexpr` (non-consteval) callers. Without
    // this wrapper, some P2996 R10 implementations reject the direct
    // call with: "constexpr variable '__range' must be initialized by a
    // constant expression — pointer to subobject of heap-allocated object
    // is not a constant expression."
    consteval auto enum_members_of(std::meta::info e) {
        return std::meta::enumerators_of(e);
    }

} // namespace enum_detail

// ---------------------------------------------------------------------------
// Concept: any scoped or unscoped enum
// ---------------------------------------------------------------------------

template <typename E>
concept enumeration = std::is_enum_v<E>;

// ---------------------------------------------------------------------------
// enum_count<E>() — number of declared enumerators
// ---------------------------------------------------------------------------

template <enumeration E>
consteval std::size_t enum_count() {
    return enum_detail::enum_members_of(^^E).size();
}

// ---------------------------------------------------------------------------
// enum_to_string — enumerator value → its declared name
// ---------------------------------------------------------------------------

template <enumeration E>
constexpr std::string_view enum_to_string(E value) {
    // Use a simple linear scan — the enumerator list is typically tiny.
    // The expansion statement unrolls this at compile time.
    template for (constexpr auto e : enum_detail::enum_members_of(^^E)) {
        if (value == [:e:])
            return std::meta::identifier_of(e);
    }
    return {};  // empty view if value doesn't match any enumerator
}

// ---------------------------------------------------------------------------
// enum_from_string<E> — string → enumerator value (returns optional)
// ---------------------------------------------------------------------------

template <enumeration E>
constexpr std::optional<E> enum_from_string(std::string_view name) {
    template for (constexpr auto e : enum_detail::enum_members_of(^^E)) {
        if (name == std::meta::identifier_of(e))
            return [:e:];
    }
    return std::nullopt;
}

// ---------------------------------------------------------------------------
// enum_names<E>() — array of all enumerator names
// ---------------------------------------------------------------------------

template <enumeration E>
consteval auto enum_names() {
    constexpr auto count = enum_count<E>();
    std::array<std::string_view, count> names{};
    std::size_t i = 0;
    template for (constexpr auto e : enum_detail::enum_members_of(^^E)) {
        names[i++] = std::meta::identifier_of(e);
    }
    return names;
}

// ---------------------------------------------------------------------------
// enum_values<E>() — array of all enumerator values
// ---------------------------------------------------------------------------

template <enumeration E>
consteval auto enum_values() {
    constexpr auto count = enum_count<E>();
    std::array<E, count> values{};
    std::size_t i = 0;
    template for (constexpr auto e : enum_detail::enum_members_of(^^E)) {
        values[i++] = [:e:];
    }
    return values;
}

// ---------------------------------------------------------------------------
// enum_entries<E>() — array of (value, name) pairs
// ---------------------------------------------------------------------------

template <enumeration E>
consteval auto enum_entries() {
    constexpr auto count = enum_count<E>();
    std::array<std::pair<E, std::string_view>, count> entries{};
    std::size_t i = 0;
    template for (constexpr auto e : enum_detail::enum_members_of(^^E)) {
        entries[i++] = {[:e:], std::meta::identifier_of(e)};
    }
    return entries;
}

// ---------------------------------------------------------------------------
// enum_contains<E> — check if an underlying integer maps to a valid enumerator
// ---------------------------------------------------------------------------

template <enumeration E>
constexpr bool enum_contains(std::underlying_type_t<E> raw) {
    template for (constexpr auto e : enum_detail::enum_members_of(^^E)) {
        if (static_cast<std::underlying_type_t<E>>([:e:]) == raw)
            return true;
    }
    return false;
}

// Overload: check by enum value
template <enumeration E>
constexpr bool enum_contains(E value) {
    return enum_contains<E>(static_cast<std::underlying_type_t<E>>(value));
}

// ---------------------------------------------------------------------------
// enum_cast<E> — safe cast from underlying type (returns optional)
// ---------------------------------------------------------------------------

template <enumeration E>
constexpr std::optional<E> enum_cast(std::underlying_type_t<E> raw) {
    template for (constexpr auto e : enum_detail::enum_members_of(^^E)) {
        if (static_cast<std::underlying_type_t<E>>([:e:]) == raw)
            return [:e:];
    }
    return std::nullopt;
}

// ---------------------------------------------------------------------------
// enum_index<E> — position of a value in the enumerator list (optional)
// ---------------------------------------------------------------------------

template <enumeration E>
constexpr std::optional<std::size_t> enum_index(E value) {
    std::size_t i = 0;
    template for (constexpr auto e : enum_detail::enum_members_of(^^E)) {
        if (value == [:e:])
            return i;
        ++i;
    }
    return std::nullopt;
}

// =========================================================================
// FLAGS / BITMASK SUPPORT
// =========================================================================
// For enums used as bitmasks (e.g. Perm::read | Perm::write).
// These functions decompose combined values into constituent flags.

// ---------------------------------------------------------------------------
// flags_to_string — decompose bitmask into "flag1|flag2|flag3"
// ---------------------------------------------------------------------------

template <enumeration E>
constexpr std::string flags_to_string(E value, std::string_view separator = "|") {
    auto raw = static_cast<std::underlying_type_t<E>>(value);
    std::string result;

    template for (constexpr auto e : enum_detail::enum_members_of(^^E)) {
        constexpr auto flag_val = static_cast<std::underlying_type_t<E>>([:e:]);
        // Skip zero-valued enumerators (e.g. "none = 0") unless value is 0
        if constexpr (flag_val != 0) {
            if ((raw & flag_val) == flag_val) {
                if (!result.empty())
                    result += separator;
                result += std::meta::identifier_of(e);
                raw &= ~flag_val;
            }
        } else {
            if (raw == 0 && static_cast<std::underlying_type_t<E>>(value) == 0) {
                return std::string(std::meta::identifier_of(e));
            }
        }
    }

    // If there are leftover bits not covered by any flag, append hex
    if (raw != 0) {
        if (!result.empty())
            result += separator;
        result += "0x";
        // Simple hex conversion for leftover bits
        constexpr char hex[] = "0123456789abcdef";
        auto uraw = static_cast<std::make_unsigned_t<std::underlying_type_t<E>>>(raw);
        bool leading = true;
        for (int shift = (sizeof(uraw) * 8 - 4); shift >= 0; shift -= 4) {
            auto nibble = (uraw >> shift) & 0xF;
            if (nibble != 0 || !leading || shift == 0) {
                result += hex[nibble];
                leading = false;
            }
        }
    }

    return result;
}

// ---------------------------------------------------------------------------
// flags_from_string — parse "flag1|flag2" into a combined enum value
// ---------------------------------------------------------------------------

template <enumeration E>
constexpr std::optional<E> flags_from_string(
    std::string_view str, std::string_view separator = "|")
{
    using U = std::underlying_type_t<E>;
    U combined = 0;

    while (!str.empty()) {
        auto pos = str.find(separator);
        auto token = str.substr(0, pos);

        // Trim whitespace
        while (!token.empty() && token.front() == ' ') token.remove_prefix(1);
        while (!token.empty() && token.back() == ' ') token.remove_suffix(1);

        auto val = enum_from_string<E>(token);
        if (!val)
            return std::nullopt;  // unknown flag name
        combined |= static_cast<U>(*val);

        if (pos == std::string_view::npos)
            break;
        str.remove_prefix(pos + separator.size());
    }

    return static_cast<E>(combined);
}

// ---------------------------------------------------------------------------
// flags_contains — check if all bits in `flag` are set in `value`
// ---------------------------------------------------------------------------

template <enumeration E>
constexpr bool flags_contains(E value, E flag) {
    using U = std::underlying_type_t<E>;
    return (static_cast<U>(value) & static_cast<U>(flag)) == static_cast<U>(flag);
}

} // namespace reflect

// ---------------------------------------------------------------------------
// Convenience: bitwise operators for any enum marked with reflect_flags
// Users can opt-in by specializing reflect::enable_bitmask_operators<E>.
// ---------------------------------------------------------------------------

namespace reflect {

template <typename E>
struct enable_bitmask_operators : std::false_type {};

template <typename E>
constexpr bool enable_bitmask_operators_v = enable_bitmask_operators<E>::value;

} // namespace reflect

template <reflect::enumeration E>
    requires reflect::enable_bitmask_operators_v<E>
constexpr E operator|(E lhs, E rhs) {
    using U = std::underlying_type_t<E>;
    return static_cast<E>(static_cast<U>(lhs) | static_cast<U>(rhs));
}

template <reflect::enumeration E>
    requires reflect::enable_bitmask_operators_v<E>
constexpr E operator&(E lhs, E rhs) {
    using U = std::underlying_type_t<E>;
    return static_cast<E>(static_cast<U>(lhs) & static_cast<U>(rhs));
}

template <reflect::enumeration E>
    requires reflect::enable_bitmask_operators_v<E>
constexpr E operator^(E lhs, E rhs) {
    using U = std::underlying_type_t<E>;
    return static_cast<E>(static_cast<U>(lhs) ^ static_cast<U>(rhs));
}

template <reflect::enumeration E>
    requires reflect::enable_bitmask_operators_v<E>
constexpr E operator~(E val) {
    using U = std::underlying_type_t<E>;
    return static_cast<E>(~static_cast<U>(val));
}

template <reflect::enumeration E>
    requires reflect::enable_bitmask_operators_v<E>
constexpr E& operator|=(E& lhs, E rhs) { return lhs = lhs | rhs; }

template <reflect::enumeration E>
    requires reflect::enable_bitmask_operators_v<E>
constexpr E& operator&=(E& lhs, E rhs) { return lhs = lhs & rhs; }

template <reflect::enumeration E>
    requires reflect::enable_bitmask_operators_v<E>
constexpr E& operator^=(E& lhs, E rhs) { return lhs = lhs ^ rhs; }

#endif // REFLECT_ENUM_HPP
