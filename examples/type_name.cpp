// examples/type_name.cpp — clean, demangled type names for any type

#include <reflect/type_name.hpp>

#include <iostream>
#include <map>
#include <string>
#include <vector>

struct MyData {
    int x;
    double y;
};

int main() {
    std::cout << "type_name<int>:                        "
              << reflect::type_name<int>() << "\n";
    std::cout << "type_name<double>:                     "
              << reflect::type_name<double>() << "\n";
    std::cout << "type_name<std::string>:                "
              << reflect::type_name<std::string>() << "\n";
    std::cout << "type_name<std::vector<int>>:           "
              << reflect::type_name<std::vector<int>>() << "\n";
    std::cout << "type_name<std::map<std::string, int>>: "
              << reflect::type_name<std::map<std::string, int>>() << "\n";
    std::cout << "type_name<MyData>:                     "
              << reflect::type_name<MyData>() << "\n";

    // Also works deduced from a value:
    int x = 42;
    std::cout << "type_name(x):                          "
              << reflect::type_name(x) << "\n";

    return 0;
}
