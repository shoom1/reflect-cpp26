// examples/generic.cpp — Composing reflect with templates.
//
// Demonstrates that reflect's APIs are not tied to a single struct type;
// they are normal templated functions that compose with the rest of the
// language. Two real-world patterns:
//
//   1. update_from<Patch>(target, patch)
//      Generic PATCH-style merge across two different struct types
//      (REST API patch endpoints, partial config updates).
//
//   2. validate(obj, rules...)
//      Field-by-field validation via a parameter pack of rules.
//
// SPDX-License-Identifier: MIT

#include <reflect/json.hpp>
#include <reflect/visit.hpp>
#include <print>
#include <string>
#include <string_view>
#include <optional>
#include <vector>

// ---------------------------------------------------------------------------
// 1. Generic PATCH merge
//
// Use case: REST PATCH endpoints. The client sends a partial update where
// only fields with values should be applied. Patch fields are
// std::optional<U> matching the target's U fields.
//
// Note the helper `try_set_field` is a separate template rather than a
// nested lambda — `template for` inside reflect::for_each_field has
// stricter capture rules than ordinary code, so flat composition keeps
// things readable.
// ---------------------------------------------------------------------------

template <typename Target, typename U>
void try_set_field(Target& target, std::string_view name, U const& value) {
    reflect::for_each_field(target, [&](std::string_view tname, auto& tval) {
        if constexpr (std::is_same_v<std::remove_cvref_t<decltype(tval)>, U>)
            if (tname == name) tval = value;
    });
}

template <typename Target, typename Patch>
void update_from(Target& target, Patch const& patch) {
    reflect::for_each_field(patch, [&](std::string_view name, auto const& opt) {
        if (opt.has_value()) try_set_field(target, name, *opt);
    });
}

struct User {
    std::string name;
    int         age;
    bool        active;
};

struct UserPatch {
    std::optional<std::string> name;
    std::optional<int>         age;
    std::optional<bool>        active;
};

// ---------------------------------------------------------------------------
// 2. Generic validation
//
// Each rule is a callable taking (field_name, value) → optional<error>.
// Rules opt-in to specific field types via `if constexpr`. The fold
// expression in validate() applies every rule to every field by
// applying one rule at a time.
// ---------------------------------------------------------------------------

struct ValidationError {
    std::string field;
    std::string message;
};

template <typename T, typename Rule>
void apply_rule(T const& obj, Rule const& rule, std::vector<ValidationError>& errors) {
    reflect::for_each_field(obj, [&](std::string_view name, auto const& value) {
        if (auto err = rule(name, value); err) errors.push_back(*err);
    });
}

template <typename T, typename... Rules>
std::vector<ValidationError> validate(T const& obj, Rules const&... rules) {
    std::vector<ValidationError> errors;
    (apply_rule(obj, rules, errors), ...);
    return errors;
}

auto non_empty_string = [](std::string_view name, auto const& v) -> std::optional<ValidationError> {
    if constexpr (std::is_same_v<std::remove_cvref_t<decltype(v)>, std::string>)
        if (v.empty()) return ValidationError{std::string(name), "must not be empty"};
    return std::nullopt;
};

auto positive_int = [](std::string_view name, auto const& v) -> std::optional<ValidationError> {
    if constexpr (std::is_same_v<std::remove_cvref_t<decltype(v)>, int>)
        if (v <= 0) return ValidationError{std::string(name), "must be positive"};
    return std::nullopt;
};

// ---------------------------------------------------------------------------

int main() {
    User u{"alice", 30, true};
    UserPatch p{.name = std::nullopt, .age = 31, .active = std::nullopt};
    update_from(u, p);
    std::println("After patch: {} (age now {})", reflect::to_json(u), u.age);

    User invalid{"", -1, true};
    auto errors = validate(invalid, non_empty_string, positive_int);
    for (auto const& e : errors)
        std::println("  validation: field '{}' {}", e.field, e.message);
}
