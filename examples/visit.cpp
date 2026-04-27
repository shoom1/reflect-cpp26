// examples/visit.cpp — generic struct field iteration

#include <reflect/visit.hpp>

#include <iostream>
#include <string_view>

struct Point {
    int x;
    int y;
};

int main() {
    Point p{10, 20};

    // Iterate every field by name + value
    std::cout << "for_each_field:\n";
    reflect::for_each_field(p, [](std::string_view name, auto const& value) {
        std::cout << "  " << name << " = " << value << "\n";
    });

    // Compile-time field metadata
    std::cout << "field_count<Point>() = "
              << reflect::field_count<Point>() << "\n";

    std::cout << "field_names<Point>(): ";
    for (auto n : reflect::field_names<Point>()) std::cout << n << " ";
    std::cout << "\n";

    // Existence check by name
    std::cout << "has_field<Point>(\"x\") = " << std::boolalpha
              << reflect::has_field<Point>("x") << "\n";
    std::cout << "has_field<Point>(\"z\") = "
              << reflect::has_field<Point>("z") << "\n";

    return 0;
}
