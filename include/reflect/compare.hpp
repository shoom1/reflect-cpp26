// reflect/compare.hpp — Automatic equality, comparison, and hashing
//
// Works on any user data class — even types you don't own. Encapsulated
// stdlib types (string, vector, optional, …) are routed to their native
// operator==/<=>/std::hash so the results are platform-portable.
//
// Usage:
//   struct Point { int x; double y; };
//
//   Point a{1, 2.0}, b{1, 2.0}, c{3, 4.0};
//
//   reflect::equal(a, b)     → true
//   reflect::equal(a, c)     → false
//   reflect::compare(a, c)   → std::strong_ordering::less
//   reflect::hash_value(a)   → std::size_t
//
//   // Use as drop-in std::hash specialization:
//   std::unordered_set<Point, reflect::hasher<Point>> points;
//
//   // Use as transparent comparators:
//   std::set<Point, reflect::less<Point>> sorted_points;
//
// SPDX-License-Identifier: MIT

#ifndef REFLECT_COMPARE_HPP
#define REFLECT_COMPARE_HPP

#include "reflect/traits.hpp"
#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <ranges>

namespace reflect {

namespace detail {

    consteval auto compare_members_of(std::meta::info t) {
        return std::meta::nonstatic_data_members_of(
            t, std::meta::access_context::current());
    }

    // Hash combiner — boost::hash_combine with a 64-bit constant. Seed
    // starts at a non-zero salt so all-zero member hashes don't collapse.
    constexpr std::size_t hash_seed = 0x9e3779b97f4a7c15ULL;

    constexpr std::size_t hash_combine(std::size_t seed, std::size_t value) {
        seed ^= value + hash_seed + (seed << 12) + (seed >> 4);
        return seed;
    }

    // Forward declaration: top-level hash dispatch
    template <typename T>
    std::size_t compute_hash(T const& value);

    // Forward declaration: top-level equality dispatch
    template <typename T>
    constexpr bool fields_equal(T const& a, T const& b);

    // ---- compute_hash dispatch ----
    template <typename T>
    std::size_t compute_hash(T const& value) {
        if constexpr (reflectable<T>) {
            std::size_t seed = hash_seed;
            template for (constexpr auto member : compare_members_of(^^T)) {
                seed = hash_combine(seed, compute_hash(value.[:member:]));
            }
            return seed;
        } else if constexpr (std::is_enum_v<T>) {
            return std::hash<std::underlying_type_t<T>>{}(
                static_cast<std::underlying_type_t<T>>(value));
        } else if constexpr (reflect::is_optional<T>) {
            return value.has_value() ? compute_hash(*value) : 0;
        } else if constexpr (reflect::is_range<T>) {
            std::size_t seed = hash_seed;
            for (auto const& elem : value)
                seed = hash_combine(seed, compute_hash(elem));
            return seed;
        } else if constexpr (requires { std::hash<T>{}(value); }) {
            return std::hash<T>{}(value);
        } else {
            static_assert(sizeof(T) == 0,
                "compute_hash: type has no std::hash specialization and "
                "is not a reflectable data class, range, or optional.");
            return 0;
        }
    }

    // ---- fields_equal dispatch ----
    template <typename T>
    constexpr bool fields_equal(T const& a, T const& b) {
        if constexpr (requires { { a == b } -> std::convertible_to<bool>; }) {
            // Native operator== takes precedence: covers strings, vectors,
            // optionals, chrono types, and any user class with ==.
            return a == b;
        } else if constexpr (reflectable<T>) {
            template for (constexpr auto member : compare_members_of(^^T)) {
                if (!fields_equal(a.[:member:], b.[:member:])) return false;
            }
            return true;
        } else if constexpr (reflect::is_range<T>) {
            return std::ranges::equal(a, b);
        } else {
            static_assert(sizeof(T) == 0,
                "equal: type has no operator== and is not reflectable.");
            return false;
        }
    }

} // namespace detail

// ---------------------------------------------------------------------------
// equal(a, b) — field-by-field equality comparison
// ---------------------------------------------------------------------------

template <reflectable T>
constexpr bool equal(T const& a, T const& b) {
    template for (constexpr auto member : detail::compare_members_of(^^T)) {
        if (!detail::fields_equal(a.[:member:], b.[:member:])) return false;
    }
    return true;
}

template <reflectable T>
constexpr bool not_equal(T const& a, T const& b) {
    return !equal(a, b);
}

// ---------------------------------------------------------------------------
// compare(a, b) — three-way comparison, field by field (left to right)
//
// Returns the common comparison category of all member comparisons.
// ---------------------------------------------------------------------------

namespace detail {

    // Three-way compare with the same dispatch table as fields_equal.
    template <typename T>
    constexpr auto fields_compare(T const& a, T const& b) {
        if constexpr (std::three_way_comparable<T>) {
            return a <=> b;
        } else if constexpr (reflectable<T>) {
            template for (constexpr auto member : compare_members_of(^^T)) {
                auto cmp = fields_compare(a.[:member:], b.[:member:]);
                if (cmp != 0) return cmp;
            }
            return std::strong_ordering::equal;
        } else {
            // Fallback: synthesize from < and ==
            if constexpr (requires { a < b; a == b; }) {
                if (a == b) return std::strong_ordering::equal;
                return a < b ? std::strong_ordering::less
                             : std::strong_ordering::greater;
            } else {
                static_assert(sizeof(T) == 0,
                    "compare: type has no <=>, no <, and is not reflectable.");
                return std::strong_ordering::equal;
            }
        }
    }

} // namespace detail

template <reflectable T>
constexpr auto compare(T const& a, T const& b) {
    template for (constexpr auto member : detail::compare_members_of(^^T)) {
        auto cmp = detail::fields_compare(a.[:member:], b.[:member:]);
        if (cmp != 0) return cmp;
    }
    return std::strong_ordering::equal;
}

// ---------------------------------------------------------------------------
// hash_value(obj) — compute a hash from all fields
// ---------------------------------------------------------------------------

template <reflectable T>
std::size_t hash_value(T const& obj) {
    return detail::compute_hash(obj);
}

// ---------------------------------------------------------------------------
// hasher<T> — drop-in for std::unordered_set/map
// ---------------------------------------------------------------------------

template <reflectable T>
struct hasher {
    std::size_t operator()(T const& obj) const {
        return hash_value(obj);
    }
};

// ---------------------------------------------------------------------------
// equal_to<T> — drop-in comparator
// ---------------------------------------------------------------------------

template <reflectable T>
struct equal_to {
    constexpr bool operator()(T const& a, T const& b) const {
        return equal(a, b);
    }
};

// ---------------------------------------------------------------------------
// less<T> / greater<T> — drop-in ordering comparators for std::set/map
// ---------------------------------------------------------------------------

template <reflectable T>
struct less {
    constexpr bool operator()(T const& a, T const& b) const {
        return compare(a, b) < 0;
    }
};

template <reflectable T>
struct greater {
    constexpr bool operator()(T const& a, T const& b) const {
        return compare(a, b) > 0;
    }
};

// ---------------------------------------------------------------------------
// first_diff_index(a, b) — index of first differing field, or field_count
// if all equal. Useful for "why aren't these equal?" debugging.
// ---------------------------------------------------------------------------

template <reflectable T>
constexpr std::size_t first_diff_index(T const& a, T const& b) {
    std::size_t idx = 0;
    template for (constexpr auto member : detail::compare_members_of(^^T)) {
        if (!detail::fields_equal(a.[:member:], b.[:member:]))
            return idx;
        ++idx;
    }
    return idx; // all equal — returns field_count
}

} // namespace reflect

#endif // REFLECT_COMPARE_HPP
