// examples/tuple.cpp — struct ↔ tuple conversion (replaces Boost.PFR)

#include <reflect/tuple.hpp>

#include <iostream>
#include <tuple>

struct Point {
    int x;
    double y;
};

int main() {
    Point p{3, 4.5};

    // Struct → tuple of values
    auto t = reflect::to_tuple(p);
    std::cout << "to_tuple(p) = (" << std::get<0>(t)
              << ", " << std::get<1>(t) << ")\n";

    // Tuple → struct (aggregate-initialization under the hood)
    auto p2 = reflect::from_tuple<Point>(std::make_tuple(42, 99.0));
    std::cout << "from_tuple<Point>({42, 99}) = ("
              << p2.x << ", " << p2.y << ")\n";

    // Index-based access
    std::cout << "get<0>(p) = " << reflect::get<0>(p) << "\n";
    std::cout << "get<1>(p) = " << reflect::get<1>(p) << "\n";

    // Apply: invoke a callable with all fields as arguments
    auto sum = reflect::apply(p, [](int x, double y) { return x + y; });
    std::cout << "apply(p, x + y) = " << sum << "\n";

    return 0;
}
