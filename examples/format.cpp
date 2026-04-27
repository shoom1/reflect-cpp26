// examples/format.cpp — std::format integration for any reflectable struct

#include <reflect/format.hpp>

#include <format>
#include <iostream>

struct Point {
    int x;
    int y;
};

// Register Point with std::formatter. Must be at namespace scope.
REFLECT_MAKE_FORMATTABLE(Point);

int main() {
    Point p{3, 7};

    // 1. reflect::format directly (no macro needed)
    std::cout << "reflect::format(p):  "
              << reflect::format(p) << "\n";
    std::cout << "json_compact:        "
              << reflect::format(p, reflect::format_detail::format_mode::json_compact)
              << "\n";

    // 2. std::format integration via REFLECT_MAKE_FORMATTABLE
    std::cout << "std::format(\"{}\",  p): " << std::format("{}",  p) << "\n";
    std::cout << "std::format(\"{:j}\", p): " << std::format("{:j}", p) << "\n";
    std::cout << "std::format(\"{:J}\", p):\n"  << std::format("{:J}", p) << "\n";

    return 0;
}
