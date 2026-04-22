// tests/test_struct.cpp — Tests for visit, print, compare, tuple, diff
//
// Compile:
//   clang++ -std=c++26 -freflection-latest -I../include test_struct.cpp -o test_struct

#include <reflect/visit.hpp>
#include <reflect/print.hpp>
#include <reflect/compare.hpp>
#include <reflect/tuple.hpp>
#include <reflect/diff.hpp>

#include <cassert>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

// ---------------------------------------------------------------------------
// Test types
// ---------------------------------------------------------------------------

struct Point { int x; int y; };
struct Vec3 { float x; float y; float z; };
struct Empty {};
struct Nested { Point origin; std::string label; };
struct WithOptional { std::string name; std::optional<int> age; };
struct Config { std::string host; int port; bool tls; };

// Nested struct without operator== — tests reflection-based comparison in diff
struct Inner { int a; int b; };
struct Outer { Inner inner; std::string tag; };

// =========================================================================
// VISIT TESTS
// =========================================================================

void test_field_count() {
    static_assert(reflect::field_count<Point>() == 2);
    static_assert(reflect::field_count<Vec3>() == 3);
    static_assert(reflect::field_count<Empty>() == 0);
    static_assert(reflect::field_count<Nested>() == 2);

    std::cout << "  field_count: PASS\n";
}

void test_field_names() {
    constexpr auto names = reflect::field_names<Point>();
    static_assert(names[0] == "x");
    static_assert(names[1] == "y");

    constexpr auto cfg_names = reflect::field_names<Config>();
    static_assert(cfg_names[0] == "host");
    static_assert(cfg_names[1] == "port");
    static_assert(cfg_names[2] == "tls");

    std::cout << "  field_names: PASS\n";
}

void test_for_each_field_const() {
    Point p{10, 20};
    std::vector<std::string> collected;
    reflect::for_each_field(p, [&](std::string_view name, auto const& value) {
        std::ostringstream os;
        os << name << "=" << value;
        collected.push_back(os.str());
    });
    assert(collected.size() == 2);
    assert(collected[0] == "x=10");
    assert(collected[1] == "y=20");

    std::cout << "  for_each_field (const): PASS\n";
}

void test_for_each_field_mutate() {
    Point p{1, 2};
    reflect::for_each_field(p, [](std::string_view, auto& value) {
        value *= 10;
    });
    assert(p.x == 10);
    assert(p.y == 20);

    std::cout << "  for_each_field (mutate): PASS\n";
}

void test_has_field() {
    static_assert(reflect::has_field<Point>("x"));
    static_assert(reflect::has_field<Point>("y"));
    static_assert(!reflect::has_field<Point>("z"));
    static_assert(!reflect::has_field<Point>(""));

    std::cout << "  has_field: PASS\n";
}

// =========================================================================
// PRINT TESTS
// =========================================================================

void test_print_simple() {
    Point p{3, 7};
    auto s = reflect::to_string(p);
    assert(s == "Point{.x=3, .y=7}");

    std::cout << "  print simple: PASS\n";
}

void test_print_nested() {
    Nested n{{1, 2}, "test"};
    auto s = reflect::to_string(n);
    assert(s == "Nested{.origin=Point{.x=1, .y=2}, .label=\"test\"}");

    std::cout << "  print nested: PASS\n";
}

void test_print_optional() {
    WithOptional a{"Alice", 30};
    WithOptional b{"Bob", std::nullopt};

    auto sa = reflect::to_string(a);
    auto sb = reflect::to_string(b);

    assert(sa.find("30") != std::string::npos);
    assert(sb.find("nullopt") != std::string::npos);

    std::cout << "  print optional: PASS\n";
}

void test_print_containers() {
    std::vector<int> v{1, 2, 3};
    auto s = reflect::to_string(v);
    assert(s == "[1, 2, 3]");

    std::vector<std::string> vs{"hello", "world"};
    auto ss = reflect::to_string(vs);
    assert(ss == "[\"hello\", \"world\"]");

    std::cout << "  print containers: PASS\n";
}

void test_print_scalars() {
    assert(reflect::to_string(true) == "true");
    assert(reflect::to_string(false) == "false");
    assert(reflect::to_string(42) == "42");
    assert(reflect::to_string('x') == "'x'");
    assert(reflect::to_string(std::string("hello")) == "\"hello\"");
    assert(reflect::to_string(nullptr) == "nullptr");

    std::cout << "  print scalars: PASS\n";
}

void test_println() {
    Point p{1, 2};
    std::ostringstream os;
    reflect::println(os, p);
    assert(os.str() == "Point{.x=1, .y=2}\n");

    std::cout << "  println: PASS\n";
}

// =========================================================================
// COMPARE TESTS
// =========================================================================

void test_equal() {
    Point a{1, 2}, b{1, 2}, c{3, 4};
    assert(reflect::equal(a, b));
    assert(!reflect::equal(a, c));
    assert(reflect::not_equal(a, c));

    // Nested
    Nested n1{{0, 0}, "same"}, n2{{0, 0}, "same"}, n3{{1, 1}, "diff"};
    assert(reflect::equal(n1, n2));
    assert(!reflect::equal(n1, n3));

    std::cout << "  equal: PASS\n";
}

void test_compare() {
    Point a{1, 2}, b{1, 3}, c{2, 0};

    assert(reflect::compare(a, a) == std::strong_ordering::equal);
    assert(reflect::compare(a, b) < 0);    // a.y < b.y
    assert(reflect::compare(a, c) < 0);    // a.x < c.x
    assert(reflect::compare(c, a) > 0);

    std::cout << "  compare: PASS\n";
}

void test_hash() {
    Point a{1, 2}, b{1, 2}, c{3, 4};

    assert(reflect::hash_value(a) == reflect::hash_value(b));
    assert(reflect::hash_value(a) != reflect::hash_value(c)); // extremely likely

    // Use in container
    std::unordered_set<Point, reflect::hasher<Point>, reflect::equal_to<Point>> s;
    s.insert({1, 2});
    s.insert({3, 4});
    s.insert({1, 2}); // duplicate
    assert(s.size() == 2);

    std::cout << "  hash: PASS\n";
}

void test_first_diff_index() {
    Point a{1, 2}, b{1, 3};
    assert(reflect::first_diff_index(a, b) == 1);  // y differs

    Point c{1, 2};
    assert(reflect::first_diff_index(a, c) == 2);  // all equal → returns field_count

    std::cout << "  first_diff_index: PASS\n";
}

void test_less_greater() {
    std::set<Point, reflect::less<Point>> sorted;
    sorted.insert({3, 1});
    sorted.insert({1, 5});
    sorted.insert({2, 3});
    auto it = sorted.begin();
    assert(it->x == 1);  // smallest x first
    ++it;
    assert(it->x == 2);
    ++it;
    assert(it->x == 3);

    std::cout << "  less/greater: PASS\n";
}

// =========================================================================
// TUPLE TESTS
// =========================================================================

void test_to_tuple() {
    Point p{10, 20};
    auto t = reflect::to_tuple(p);
    assert(std::get<0>(t) == 10);
    assert(std::get<1>(t) == 20);

    Vec3 v{1.0f, 2.0f, 3.0f};
    auto vt = reflect::to_tuple(v);
    assert(std::get<2>(vt) == 3.0f);

    std::cout << "  to_tuple: PASS\n";
}

void test_from_tuple() {
    auto p = reflect::from_tuple<Point>(std::make_tuple(42, 99));
    assert(p.x == 42);
    assert(p.y == 99);

    std::cout << "  from_tuple: PASS\n";
}

void test_get() {
    Point p{5, 10};
    assert(reflect::get<0>(p) == 5);
    assert(reflect::get<1>(p) == 10);

    // Mutable get
    reflect::get<0>(p) = 100;
    assert(p.x == 100);

    std::cout << "  get<I>: PASS\n";
}

void test_apply() {
    Point p{3, 4};
    auto sum = reflect::apply(p, [](int x, int y) { return x + y; });
    assert(sum == 7);

    auto product = reflect::apply(p, [](int x, int y) { return x * y; });
    assert(product == 12);

    std::cout << "  apply: PASS\n";
}

void test_to_ref_tuple() {
    Point p{1, 2};
    auto refs = reflect::to_ref_tuple(p);
    std::get<0>(refs) = 100;
    assert(p.x == 100);

    std::cout << "  to_ref_tuple: PASS\n";
}

void test_roundtrip() {
    Point original{42, 99};
    auto t = reflect::to_tuple(original);
    auto restored = reflect::from_tuple<Point>(t);
    assert(reflect::equal(original, restored));

    std::cout << "  roundtrip: PASS\n";
}

// =========================================================================
// DIFF TESTS
// =========================================================================

void test_diff_basic() {
    Config a{"localhost", 8080, false};
    Config b{"localhost", 9090, true};

    auto changes = reflect::diff(a, b);
    assert(changes.size() == 3);
    assert(!changes[0].changed);  // host same
    assert(changes[1].changed);   // port changed
    assert(changes[2].changed);   // tls changed

    std::cout << "  diff basic: PASS\n";
}

void test_diff_no_changes() {
    Point a{1, 2}, b{1, 2};
    assert(!reflect::has_changes(a, b));
    assert(reflect::change_count(a, b) == 0);

    std::cout << "  diff no changes: PASS\n";
}

void test_changed_field_names() {
    Config a{"localhost", 8080, false};
    Config b{"localhost", 9090, true};

    auto names = reflect::changed_field_names(a, b);
    assert(names.size() == 2);
    assert(names[0] == "port");
    assert(names[1] == "tls");

    std::cout << "  changed_field_names: PASS\n";
}

void test_diff_summary() {
    Config a{"localhost", 8080, false};
    Config b{"localhost", 9090, true};

    auto summary = reflect::diff_summary(a, b);
    assert(summary.find("port") != std::string::npos);
    assert(summary.find("8080") != std::string::npos);
    assert(summary.find("9090") != std::string::npos);
    assert(summary.find("tls") != std::string::npos);

    // No changes
    auto no_changes = reflect::diff_summary(a, a);
    assert(no_changes == "(no changes)");

    std::cout << "  diff_summary: PASS\n";
}

void test_apply_changes() {
    Config a{"localhost", 8080, false};
    Config b{"remotehost", 9090, true};

    reflect::apply_changes(a, b);
    assert(a.host == "remotehost");
    assert(a.port == 9090);
    assert(a.tls == true);

    std::cout << "  apply_changes: PASS\n";
}

void test_diff_nested_no_eq() {
    Outer a{{1, 2}, "same"};
    Outer b{{1, 2}, "same"};
    Outer c{{1, 3}, "same"};

    assert(!reflect::has_changes(a, b));
    assert(reflect::has_changes(a, c));
    assert(reflect::change_count(a, c) == 1); // inner changed

    auto names = reflect::changed_field_names(a, c);
    assert(names.size() == 1);
    assert(names[0] == "inner");

    std::cout << "  diff nested (no operator==): PASS\n";
}

void test_apply_changes_predicate() {
    Config a{"localhost", 8080, false};
    Config b{"remotehost", 9090, true};

    // Only apply changes to integer fields
    reflect::apply_changes(a, b, [](auto, auto const& old_val, auto const& new_val) {
        if constexpr (std::is_same_v<std::remove_cvref_t<decltype(old_val)>, int>)
            return old_val != new_val;
        return false;
    });
    assert(a.host == "localhost");  // NOT changed (string)
    assert(a.port == 9090);        // changed (int)
    assert(a.tls == false);        // NOT changed (bool)

    std::cout << "  apply_changes (predicate): PASS\n";
}

// =========================================================================
// MAIN
// =========================================================================

int main() {
    std::cout << "=== test_struct — visit ===\n";
    test_field_count();
    test_field_names();
    test_for_each_field_const();
    test_for_each_field_mutate();
    test_has_field();

    std::cout << "\n=== test_struct — print ===\n";
    test_print_simple();
    test_print_nested();
    test_print_optional();
    test_print_containers();
    test_print_scalars();
    test_println();

    std::cout << "\n=== test_struct — compare ===\n";
    test_equal();
    test_compare();
    test_hash();
    test_first_diff_index();
    test_less_greater();

    std::cout << "\n=== test_struct — tuple ===\n";
    test_to_tuple();
    test_from_tuple();
    test_get();
    test_apply();
    test_to_ref_tuple();
    test_roundtrip();

    std::cout << "\n=== test_struct — diff ===\n";
    test_diff_basic();
    test_diff_no_changes();
    test_changed_field_names();
    test_diff_summary();
    test_diff_nested_no_eq();
    test_apply_changes();
    test_apply_changes_predicate();

    std::cout << "\nAll struct tests passed!\n";
    return 0;
}
