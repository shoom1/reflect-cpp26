// tests/test_enum.cpp — Tests for reflect/enum.hpp
//
// Compile:
//   clang++ -std=c++26 -freflection-latest -I../include test_enum.cpp -o test_enum

#include <reflect/enum.hpp>
#include <cassert>
#include <iostream>
#include <string_view>

// ---------------------------------------------------------------------------
// Test enums
// ---------------------------------------------------------------------------

enum class Color { red, green, blue };

enum class Status : int { pending = 0, active = 1, closed = 2 };

enum class Sparse { a = 10, b = 20, c = 100 };

enum Unscoped { up, down, left, right };

enum class Perm : unsigned {
    none  = 0,
    read  = 1,
    write = 2,
    exec  = 4,
    all   = 7,
};
template <> struct reflect::enable_bitmask_operators<Perm> : std::true_type {};

enum class Empty {};

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

void test_enum_to_string() {
    assert(reflect::enum_to_string(Color::red) == "red");
    assert(reflect::enum_to_string(Color::green) == "green");
    assert(reflect::enum_to_string(Color::blue) == "blue");

    assert(reflect::enum_to_string(Status::pending) == "pending");
    assert(reflect::enum_to_string(Status::active) == "active");
    assert(reflect::enum_to_string(Status::closed) == "closed");

    assert(reflect::enum_to_string(Sparse::a) == "a");
    assert(reflect::enum_to_string(Sparse::c) == "c");

    // Invalid value returns empty
    assert(reflect::enum_to_string(static_cast<Color>(99)).empty());

    std::cout << "  enum_to_string: PASS\n";
}

void test_enum_from_string() {
    assert(reflect::enum_from_string<Color>("red") == Color::red);
    assert(reflect::enum_from_string<Color>("blue") == Color::blue);
    assert(reflect::enum_from_string<Color>("invalid") == std::nullopt);
    assert(reflect::enum_from_string<Color>("") == std::nullopt);

    assert(reflect::enum_from_string<Status>("active") == Status::active);

    std::cout << "  enum_from_string: PASS\n";
}

void test_enum_count() {
    static_assert(reflect::enum_count<Color>() == 3);
    static_assert(reflect::enum_count<Status>() == 3);
    static_assert(reflect::enum_count<Sparse>() == 3);
    static_assert(reflect::enum_count<Perm>() == 5);
    static_assert(reflect::enum_count<Empty>() == 0);

    std::cout << "  enum_count: PASS\n";
}

void test_enum_names() {
    constexpr auto names = reflect::enum_names<Color>();
    static_assert(names.size() == 3);
    static_assert(names[0] == "red");
    static_assert(names[1] == "green");
    static_assert(names[2] == "blue");

    std::cout << "  enum_names: PASS\n";
}

void test_enum_values() {
    constexpr auto values = reflect::enum_values<Color>();
    static_assert(values.size() == 3);
    static_assert(values[0] == Color::red);
    static_assert(values[1] == Color::green);
    static_assert(values[2] == Color::blue);

    std::cout << "  enum_values: PASS\n";
}

void test_enum_entries() {
    constexpr auto entries = reflect::enum_entries<Color>();
    static_assert(entries[0].first == Color::red);
    static_assert(entries[0].second == "red");

    std::cout << "  enum_entries: PASS\n";
}

void test_enum_contains() {
    assert(reflect::enum_contains<Color>(0));   // red
    assert(reflect::enum_contains<Color>(1));   // green
    assert(reflect::enum_contains<Color>(2));   // blue
    assert(!reflect::enum_contains<Color>(99));

    assert(reflect::enum_contains(Color::red));

    assert(reflect::enum_contains<Sparse>(10));
    assert(!reflect::enum_contains<Sparse>(15));

    std::cout << "  enum_contains: PASS\n";
}

void test_enum_cast() {
    assert(reflect::enum_cast<Color>(0) == Color::red);
    assert(reflect::enum_cast<Color>(2) == Color::blue);
    assert(reflect::enum_cast<Color>(99) == std::nullopt);

    std::cout << "  enum_cast: PASS\n";
}

void test_enum_index() {
    assert(reflect::enum_index(Color::red) == 0);
    assert(reflect::enum_index(Color::green) == 1);
    assert(reflect::enum_index(Color::blue) == 2);
    assert(reflect::enum_index(static_cast<Color>(99)) == std::nullopt);

    std::cout << "  enum_index: PASS\n";
}

void test_flags() {
    auto perms = Perm::read | Perm::exec;
    auto str = reflect::flags_to_string(perms);
    assert(str == "read|exec");

    auto zero = reflect::flags_to_string(Perm::none);
    assert(zero == "none");

    auto all = reflect::flags_to_string(Perm::all);
    // "all" should match since 7 == read|write|exec, but the enumerator "all" covers it
    assert(!all.empty());

    // Parse flags
    auto parsed = reflect::flags_from_string<Perm>("read|write");
    assert(parsed.has_value());
    assert(reflect::flags_contains(*parsed, Perm::read));
    assert(reflect::flags_contains(*parsed, Perm::write));
    assert(!reflect::flags_contains(*parsed, Perm::exec));

    // Invalid flag
    assert(reflect::flags_from_string<Perm>("read|bogus") == std::nullopt);

    std::cout << "  flags: PASS\n";
}

void test_bitmask_operators() {
    auto rw = Perm::read | Perm::write;
    assert(static_cast<unsigned>(rw) == 3);
    assert((rw & Perm::read) == Perm::read);
    assert((rw ^ Perm::read) == Perm::write);
    auto neg = ~Perm::read;
    assert((neg & Perm::write) == Perm::write);

    Perm p = Perm::read;
    p |= Perm::exec;
    assert(static_cast<unsigned>(p) == 5);

    std::cout << "  bitmask_operators: PASS\n";
}

void test_unscoped_enum() {
    assert(reflect::enum_to_string(up) == "up");
    assert(reflect::enum_to_string(right) == "right");
    assert(reflect::enum_from_string<Unscoped>("down") == down);
    static_assert(reflect::enum_count<Unscoped>() == 4);

    std::cout << "  unscoped_enum: PASS\n";
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main() {
    std::cout << "=== test_enum ===\n";
    test_enum_to_string();
    test_enum_from_string();
    test_enum_count();
    test_enum_names();
    test_enum_values();
    test_enum_entries();
    test_enum_contains();
    test_enum_cast();
    test_enum_index();
    test_flags();
    test_bitmask_operators();
    test_unscoped_enum();
    std::cout << "All enum tests passed!\n\n";
    return 0;
}
