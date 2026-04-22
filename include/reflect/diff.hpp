// reflect/diff.hpp — Structural diff between two instances of the same type
//
// Usage:
//   struct Config { std::string host; int port; bool tls; };
//   Config a{"localhost", 8080, false};
//   Config b{"localhost", 9090, true};
//
//   auto changes = reflect::diff(a, b);
//   // changes[0] = {.name="port",  .index=1, .changed=true}
//   // changes[1] = {.name="tls",   .index=2, .changed=true}
//
//   reflect::diff_summary(a, b);
//   // → "port: 8080 → 9090, tls: false → true"
//
//   reflect::has_changes(a, b); // → true
//   reflect::changed_field_names(a, b); // → {"port", "tls"}
//
// SPDX-License-Identifier: MIT

#ifndef REFLECT_DIFF_HPP
#define REFLECT_DIFF_HPP

#include "reflect/traits.hpp"
#include <charconv>
#include <cstddef>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

namespace reflect {

// ---------------------------------------------------------------------------
// diff_entry — describes whether a single field changed
// ---------------------------------------------------------------------------

struct diff_entry {
    std::string_view name;
    std::size_t index;
    bool changed;
};

namespace diff_detail {

    consteval auto diff_members_of(std::meta::info t) {
        return std::meta::nonstatic_data_members_of(
            t, std::meta::access_context::current());
    }

    // Compare a single value — covers stdlib encapsulated types via
    // their native operator==, recurses for nested reflectable structs,
    // and falls through to range equality where possible.
    template <typename T>
    constexpr bool values_equal(T const& a, T const& b) {
        if constexpr (requires { { a == b } -> std::convertible_to<bool>; }) {
            return a == b;
        } else if constexpr (reflectable<T>) {
            template for (constexpr auto m : diff_members_of(^^T)) {
                if (!values_equal(a.[:m:], b.[:m:])) return false;
            }
            return true;
        } else if constexpr (reflect::is_range<T>) {
            return std::ranges::equal(a, b);
        } else {
            static_assert(sizeof(T) == 0,
                "diff: type has no operator== and is not reflectable.");
            return false;
        }
    }

    // Render a value for diff_summary. Uses to_chars for arithmetic,
    // string passthrough for strings, falls back to "(?)".
    template <typename T>
    std::string value_to_string(T const& val) {
        if constexpr (std::is_same_v<std::remove_cvref_t<T>, bool>) {
            return val ? "true" : "false";
        } else if constexpr (std::is_arithmetic_v<T>) {
            char buf[64];
            auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), val);
            return std::string(buf, static_cast<std::size_t>(ptr - buf));
        } else if constexpr (reflect::is_string_like<T>) {
            return std::string(val);
        } else {
            return "(?)";
        }
    }

} // namespace diff_detail

// ---------------------------------------------------------------------------
// diff(a, b) — vector of diff_entry, one per field
// ---------------------------------------------------------------------------

template <reflectable T>
std::vector<diff_entry> diff(T const& a, T const& b) {
    std::vector<diff_entry> entries;
    std::size_t idx = 0;

    template for (constexpr auto member : diff_detail::diff_members_of(^^T)) {
        bool eq = diff_detail::values_equal(a.[:member:], b.[:member:]);
        entries.push_back({
            .name = std::meta::identifier_of(member),
            .index = idx,
            .changed = !eq
        });
        ++idx;
    }

    return entries;
}

// ---------------------------------------------------------------------------
// changed_fields(a, b) — only the entries that changed
// ---------------------------------------------------------------------------

template <reflectable T>
std::vector<diff_entry> changed_fields(T const& a, T const& b) {
    auto all = diff(a, b);
    std::vector<diff_entry> result;
    for (auto const& e : all) {
        if (e.changed) result.push_back(e);
    }
    return result;
}

// ---------------------------------------------------------------------------
// changed_field_names(a, b) — just the names
// ---------------------------------------------------------------------------

template <reflectable T>
std::vector<std::string_view> changed_field_names(T const& a, T const& b) {
    auto all = diff(a, b);
    std::vector<std::string_view> names;
    for (auto const& e : all) {
        if (e.changed) names.push_back(e.name);
    }
    return names;
}

// ---------------------------------------------------------------------------
// has_changes(a, b) — fast path: stop at first difference
// ---------------------------------------------------------------------------

template <reflectable T>
bool has_changes(T const& a, T const& b) {
    template for (constexpr auto member : diff_detail::diff_members_of(^^T)) {
        if (!diff_detail::values_equal(a.[:member:], b.[:member:]))
            return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// change_count(a, b) — how many fields differ?
// ---------------------------------------------------------------------------

template <reflectable T>
std::size_t change_count(T const& a, T const& b) {
    std::size_t count = 0;
    template for (constexpr auto member : diff_detail::diff_members_of(^^T)) {
        if (!diff_detail::values_equal(a.[:member:], b.[:member:]))
            ++count;
    }
    return count;
}

// ---------------------------------------------------------------------------
// diff_summary(a, b) — human-readable string: "field: old → new, ..."
// ---------------------------------------------------------------------------

template <reflectable T>
std::string diff_summary(T const& a, T const& b) {
    std::string out;
    bool first = true;

    template for (constexpr auto member : diff_detail::diff_members_of(^^T)) {
        if (!diff_detail::values_equal(a.[:member:], b.[:member:])) {
            if (!first) out += ", ";
            first = false;
            out += std::meta::identifier_of(member);
            out += ": ";
            out += diff_detail::value_to_string(a.[:member:]);
            out += " → ";
            out += diff_detail::value_to_string(b.[:member:]);
        }
    }

    return first ? std::string("(no changes)") : out;
}

// ---------------------------------------------------------------------------
// apply_changes(target, source, predicate) — selectively copy fields
// ---------------------------------------------------------------------------

template <reflectable T, typename Pred>
void apply_changes(T& target, T const& source, Pred&& pred) {
    template for (constexpr auto member : diff_detail::diff_members_of(^^T)) {
        constexpr auto name = std::meta::identifier_of(member);
        if (pred(name, target.[:member:], source.[:member:])) {
            target.[:member:] = source.[:member:];
        }
    }
}

// Overload: copy ALL changed fields unconditionally
template <reflectable T>
void apply_changes(T& target, T const& source) {
    apply_changes(target, source, [](auto, auto const& old_val, auto const& new_val) {
        return !diff_detail::values_equal(old_val, new_val);
    });
}

} // namespace reflect

#endif // REFLECT_DIFF_HPP
