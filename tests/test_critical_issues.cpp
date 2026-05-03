// tests/test_critical_issues.cpp — regression test for the issues
// described in docs/notes/code-review-2026-04.md. Once the fixes land,
// every section in this file should succeed; if any one regresses, the
// section reports the observed wrong value alongside the expected one.
//
// Built and run via ctest (registered by CMakeLists.txt's tests glob).

#include <reflect/reflect.hpp>

#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace {

int failures = 0;

void section(std::string_view title) {
    std::cout << "\n=== " << title << " ===\n";
}

void check(bool ok, std::string_view label) {
    std::cout << (ok ? "  PASS  " : "  FAIL  ") << label << "\n";
    if (!ok) ++failures;
}

// ===========================================================================
// C1 — `reflect::reflectable` is no longer too broad
// ===========================================================================
// After the fix, `reflectable<T>` is a structural test: T is a class whose
// non-static data members are all public. Stdlib types with private state
// (string, vector, optional, path, function, …) are excluded, and the
// library routes them through their public APIs (==, std::hash, range
// iteration) instead of recursing into implementation details.

void c1_reflectable_predicate() {
    section("C1: reflectable concept narrowed to true data classes");

    static_assert(!reflect::reflectable<std::string>,
        "std::string has private state — must NOT be reflectable.");
    static_assert(!reflect::reflectable<std::vector<int>>,
        "std::vector has private state — must NOT be reflectable.");
    static_assert(!reflect::reflectable<std::optional<int>>,
        "std::optional has private state — must NOT be reflectable.");

    struct Point { int x; int y; };
    static_assert(reflect::reflectable<Point>,
        "Plain user struct must be reflectable.");

    check(true, "concept membership static_asserts compiled");

    // Structs containing stdlib fields now work correctly: equal/hash/diff
    // dispatch to the field's native operations rather than recursing.
    struct Order {
        int id;
        std::string symbol;
        std::vector<int> fills;
    };

    Order a{1, "AAPL", {100, 200}};
    Order b{1, "AAPL", {100, 200}};

    check(reflect::equal(a, b),
          "equal(a, b) for struct with std::string and std::vector fields");
    check(reflect::hash_value(a) == reflect::hash_value(b),
          "hash_value(a) == hash_value(b) for struct with stdlib fields");
    check(!reflect::has_changes(a, b),
          "has_changes(a, b) is false for equal Orders");

    Order c{1, "AAPL", {100, 999}};
    check(!reflect::equal(a, c), "equal(a, c) detects vector-element change");
    check(reflect::change_count(a, c) == 1, "change_count(a, c) == 1");
}

// ===========================================================================
// C2 — JSON parser preserves integer precision
// ===========================================================================

struct Trade { std::int64_t id; };

void c2_int_round_trip() {
    section("C2: JSON parser preserves integers > 2^53");

    Trade orig{(std::int64_t{1} << 53) + 1};   // 9 007 199 254 740 993
    auto json = reflect::to_json(orig);
    auto back = reflect::from_json<Trade>(json);

    std::cout << "  original   : " << orig.id << "\n";
    std::cout << "  serialized : " << json << "\n";
    std::cout << "  round-trip : " << back.id << "\n";

    check(back.id == orig.id, "int64 above 2^53 round-trips losslessly");
}

// ===========================================================================
// C3 — JSON parser rejects whitespace-split keywords
// ===========================================================================

struct Flag { bool ok; };

void c3_strict_tokenization() {
    section("C3: JSON parser rejects whitespace-split keywords");

    constexpr std::string_view bad = R"({"ok": t r u e})";
    bool threw = false;
    try {
        auto v = reflect::from_json<Flag>(bad);
        std::cout << "  parsed as: ok=" << std::boolalpha << v.ok << "\n";
    } catch (reflect::json_parse_error const& e) {
        threw = true;
        std::cout << "  rejected with: " << e.what() << "\n";
    }
    check(threw, "`t r u e` is rejected as malformed JSON");

    // Sanity: well-formed JSON still parses.
    auto good = reflect::from_json<Flag>(R"({"ok": true})");
    check(good.ok, "well-formed `true` still parses");
}

// ===========================================================================
// C4 — JSON parser enforces a recursion depth limit
// ===========================================================================

void c4_depth_limit() {
    section("C4: JSON parser caps recursion depth");

    // Shallow nesting still works.
    auto shallow = reflect::from_json<std::vector<std::vector<std::vector<int>>>>(
        "[[[1,2,3]]]");
    check(shallow.size() == 1 && shallow[0].size() == 1 && shallow[0][0].size() == 3,
          "shallow nested arrays parse");

    // The depth guard sits in parse_value for object/array/map and in
    // skip_value (used to discard unknown keys). The cleanest way to
    // exercise it from user code is to send a deeply-nested payload as
    // an unknown field of a tiny struct — skip_value recurses on each
    // open bracket and trips the limit.
    struct Empty {};
    constexpr std::size_t depth = 2048;
    std::string payload = R"({"deep":)";
    payload.append(depth, '[');
    payload.append(depth, ']');
    payload += '}';

    bool threw = false;
    try {
        auto v = reflect::from_json<Empty>(payload);
        (void)v;
    } catch (reflect::json_parse_error const& e) {
        threw = true;
        std::cout << "  caught: " << e.what() << "\n";
    }
    check(threw, "depth-2048 payload is rejected before stack overflow");
}

// ===========================================================================
// C5 — to_json refuses to emit NaN / Inf
// ===========================================================================

struct Measurement { double value; };

void c5_nan_serialization() {
    section("C5: to_json refuses non-finite floats");

    Measurement nan{std::numeric_limits<double>::quiet_NaN()};
    Measurement inf{std::numeric_limits<double>::infinity()};

    bool nan_threw = false;
    try { reflect::to_json(nan); }
    catch (std::exception const& e) {
        nan_threw = true;
        std::cout << "  NaN: " << e.what() << "\n";
    }
    check(nan_threw, "to_json(NaN) throws");

    bool inf_threw = false;
    try { reflect::to_json(inf); }
    catch (std::exception const& e) {
        inf_threw = true;
        std::cout << "  Inf: " << e.what() << "\n";
    }
    check(inf_threw, "to_json(Inf) throws");

    // Sanity: finite floats still serialize.
    Measurement ok{3.14};
    auto j = reflect::to_json(ok);
    check(j.find("3.14") != std::string::npos, "finite double still serializes");
}

} // anonymous namespace

int main() {
    std::cout << "=== reflect — critical-issue regression test ===\n";

    c1_reflectable_predicate();
    c2_int_round_trip();
    c3_strict_tokenization();
    c4_depth_limit();
    c5_nan_serialization();

    std::cout << "\n=== "
              << (failures == 0 ? "all checks passed" : "FAILURES: ")
              << (failures == 0 ? "" : std::to_string(failures))
              << " ===\n";
    return failures == 0 ? 0 : 1;
}
