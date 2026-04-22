// examples/showcase.cpp — Everything reflect can do, in one file
//
// Compile with Bloomberg Clang fork:
//   clang++ -std=c++26 -freflection-latest -I../include showcase.cpp -o showcase
//
// Or with GCC trunk (16+):
//   g++ -std=c++26 -I../include showcase.cpp -o showcase

#include <reflect/reflect.hpp>

#include <iostream>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

// ---------------------------------------------------------------------------
// Example types
// ---------------------------------------------------------------------------

enum class Color { red, green, blue };

enum class Perm : unsigned {
    none  = 0,
    read  = 1,
    write = 2,
    exec  = 4,
};
template <> struct reflect::enable_bitmask_operators<Perm> : std::true_type {};

struct Point {
    int x;
    int y;
};

struct Person {
    std::string name;
    int age;
    std::optional<std::string> email;
};

struct Trade {
    int id;
    std::string symbol;
    double price;
    int quantity;
    Color side;
    std::vector<std::string> tags;
};

struct Nested {
    Point origin;
    std::vector<Point> waypoints;
    std::string label;
};

// ===========================================================================
int main() {
    std::cout << "=== reflect — C++26 Reflection Utilities ===\n\n";

    // -----------------------------------------------------------------------
    // 1. TYPE NAMES
    // -----------------------------------------------------------------------
    std::cout << "--- type_name ---\n";
    std::cout << "int:                " << reflect::type_name<int>() << "\n";
    std::cout << "std::vector<Point>: " << reflect::type_name<std::vector<Point>>() << "\n";
    std::cout << "Color:              " << reflect::type_name<Color>() << "\n\n";

    // -----------------------------------------------------------------------
    // 2. ENUM REFLECTION
    // -----------------------------------------------------------------------
    std::cout << "--- enum ---\n";

    // to_string / from_string
    std::cout << "Color::green → \"" << reflect::enum_to_string(Color::green) << "\"\n";
    auto parsed = reflect::enum_from_string<Color>("blue");
    std::cout << "\"blue\" → Color::" << reflect::enum_to_string(*parsed) << "\n";

    // count, names, values
    std::cout << "Color has " << reflect::enum_count<Color>() << " values: ";
    for (auto name : reflect::enum_names<Color>())
        std::cout << name << " ";
    std::cout << "\n";

    // contains / cast
    std::cout << "enum_contains<Color>(1) → " << std::boolalpha
              << reflect::enum_contains<Color>(1) << "\n";
    std::cout << "enum_contains<Color>(99) → "
              << reflect::enum_contains<Color>(99) << "\n";

    // flags
    auto perms = Perm::read | Perm::exec;
    std::cout << "Perm::read|exec → \"" << reflect::flags_to_string(perms) << "\"\n";
    auto parsed_flags = reflect::flags_from_string<Perm>("read|write");
    std::cout << "\"read|write\" → " << reflect::flags_to_string(*parsed_flags) << "\n\n";

    // -----------------------------------------------------------------------
    // 3. STRUCT ITERATION (visit)
    // -----------------------------------------------------------------------
    std::cout << "--- for_each_field ---\n";
    Point p{10, 20};
    reflect::for_each_field(p, [](std::string_view name, auto const& value) {
        std::cout << "  " << name << " = " << value << "\n";
    });
    std::cout << "Point has " << reflect::field_count<Point>() << " fields: ";
    for (auto name : reflect::field_names<Point>())
        std::cout << name << " ";
    std::cout << "\n\n";

    // -----------------------------------------------------------------------
    // 4. PRETTY PRINT
    // -----------------------------------------------------------------------
    std::cout << "--- to_string (pretty print) ---\n";

    Person alice{"Alice", 30, "alice@example.com"};
    Person bob{"Bob", 25, std::nullopt};
    std::cout << reflect::to_string(alice) << "\n";
    std::cout << reflect::to_string(bob) << "\n";

    Nested route{
        {0, 0},
        {{1, 2}, {3, 4}, {5, 6}},
        "scenic route"
    };
    std::cout << reflect::to_string(route) << "\n\n";

    // Debug macro
    REFLECT_DEBUG(alice);
    std::cout << "\n";

    // -----------------------------------------------------------------------
    // 5. COMPARISON & HASHING
    // -----------------------------------------------------------------------
    std::cout << "--- compare & hash ---\n";

    Point a{1, 2}, b{1, 2}, c{3, 4};
    std::cout << "equal(a, b) → " << reflect::equal(a, b) << "\n";
    std::cout << "equal(a, c) → " << reflect::equal(a, c) << "\n";
    std::cout << "compare(a, c) < 0 → " << (reflect::compare(a, c) < 0) << "\n";
    std::cout << "hash(a) = " << reflect::hash_value(a) << "\n";
    std::cout << "hash(b) = " << reflect::hash_value(b) << "  (same as a)\n";
    std::cout << "hash(c) = " << reflect::hash_value(c) << "  (different)\n";

    // Use in unordered_set
    std::unordered_set<Point, reflect::hasher<Point>, reflect::equal_to<Point>> point_set;
    point_set.insert({1, 2});
    point_set.insert({3, 4});
    point_set.insert({1, 2}); // duplicate
    std::cout << "unordered_set size (after inserting 3, with 1 dup): "
              << point_set.size() << "\n\n";

    // -----------------------------------------------------------------------
    // 6. TUPLE CONVERSION
    // -----------------------------------------------------------------------
    std::cout << "--- tuple ---\n";

    auto tup = reflect::to_tuple(p);
    std::cout << "to_tuple(Point{10,20}) → (" << std::get<0>(tup)
              << ", " << std::get<1>(tup) << ")\n";

    auto p2 = reflect::from_tuple<Point>(std::make_tuple(42, 99));
    std::cout << "from_tuple<Point>({42, 99}) → " << reflect::to_string(p2) << "\n";

    std::cout << "get<0>(p) = " << reflect::get<0>(p) << "\n";
    std::cout << "get<1>(p) = " << reflect::get<1>(p) << "\n";

    auto sum = reflect::apply(p, [](int x, int y) { return x + y; });
    std::cout << "apply(p, x+y) = " << sum << "\n\n";

    // -----------------------------------------------------------------------
    // 7. JSON SERIALIZATION
    // -----------------------------------------------------------------------
    std::cout << "--- JSON ---\n";

    Trade trade{1, "AAPL", 198.50, 100, Color::green, {"tech", "earnings"}};
    auto json = reflect::to_json(trade);
    std::cout << "Compact:  " << json << "\n";

    auto pretty = reflect::to_json(trade, {.indent = 2});
    std::cout << "Pretty:\n" << pretty << "\n";

    // Round-trip
    auto trade2 = reflect::from_json<Trade>(json);
    std::cout << "Round-trip symbol: " << trade2.symbol
              << ", price: " << trade2.price
              << ", side: " << reflect::enum_to_string(trade2.side)
              << "\n";

    // Nested struct JSON
    auto nested_json = reflect::to_json(route, {.indent = 2});
    std::cout << "\nNested JSON:\n" << nested_json << "\n";

    // skip_nullopt option
    auto bob_json = reflect::to_json(bob, {.skip_nullopt = true});
    std::cout << "Bob (skip_nullopt): " << bob_json << "\n\n";

    std::cout << "=== All examples passed! ===\n";
    return 0;
}
