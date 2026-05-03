// Smoke test: a downstream consumer that uses a couple of reflect
// features through the installed package. If any header fails to find
// its companions, or if -freflection-latest isn't reaching the consumer
// build, this won't compile or won't link.

#include <reflect/json.hpp>
#include <reflect/print.hpp>

#include <iostream>
#include <string>

struct Point { int x; int y; };

int main() {
    Point p{3, 4};

    auto json = reflect::to_json(p);
    auto pretty = reflect::to_string(p);

    if (json != R"({"x":3,"y":4})") {
        std::cerr << "to_json mismatch: " << json << "\n";
        return 1;
    }
    if (pretty != "Point{.x=3, .y=4}") {
        std::cerr << "to_string mismatch: " << pretty << "\n";
        return 1;
    }

    std::cout << "install smoke test passed\n";
    return 0;
}
