// examples/print.cpp — Rust-Debug-style pretty-printing for any struct

#include <reflect/print.hpp>

#include <iostream>
#include <optional>
#include <string>
#include <vector>

struct Point {
    int x;
    int y;
};

struct Person {
    std::string name;
    int age;
    std::optional<std::string> email;
    std::vector<Point> waypoints;
};

int main() {
    Point p{3, 7};
    Person alice{"Alice", 30, "alice@example.com", {{0, 0}, {1, 2}, {3, 4}}};
    Person bob{"Bob", 25, std::nullopt, {}};

    // to_string returns the formatted text
    std::cout << "to_string(p):     " << reflect::to_string(p)     << "\n";
    std::cout << "to_string(alice): " << reflect::to_string(alice) << "\n";
    std::cout << "to_string(bob):   " << reflect::to_string(bob)   << "\n";

    // print / println write directly to an ostream
    std::cout << "println(p):       ";
    reflect::println(std::cout, p);

    // REFLECT_DEBUG: prints "expr = value" — handy for ad-hoc debugging
    REFLECT_DEBUG(alice);

    return 0;
}
