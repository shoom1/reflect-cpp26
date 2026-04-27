// examples/compare.cpp — automatic ==, <=>, std::hash for any data class

#include <reflect/compare.hpp>

#include <iostream>
#include <set>
#include <unordered_set>

struct Point {
    int x;
    int y;
};

int main() {
    Point a{1, 2}, b{1, 2}, c{3, 4};

    // Equality and ordering
    std::cout << "equal(a, b)         = " << std::boolalpha
              << reflect::equal(a, b) << "\n";
    std::cout << "equal(a, c)         = " << reflect::equal(a, c) << "\n";
    std::cout << "compare(a, c) < 0   = " << (reflect::compare(a, c) < 0) << "\n";
    std::cout << "compare(c, a) > 0   = " << (reflect::compare(c, a) > 0) << "\n";

    // Hashing — equal values produce equal hashes
    std::cout << "hash_value(a) == hash_value(b): "
              << (reflect::hash_value(a) == reflect::hash_value(b)) << "\n";

    // Drop-in functors for unordered containers
    std::unordered_set<Point, reflect::hasher<Point>, reflect::equal_to<Point>> uset;
    uset.insert({1, 2});
    uset.insert({3, 4});
    uset.insert({1, 2});  // duplicate — ignored
    std::cout << "unordered_set size after 3 inserts (1 dup): "
              << uset.size() << "\n";

    // Drop-in functors for ordered containers
    std::set<Point, reflect::less<Point>> sorted;
    sorted.insert({3, 4});
    sorted.insert({1, 2});
    sorted.insert({2, 3});
    std::cout << "std::set sorted by reflect::less<Point>: ";
    for (auto const& p : sorted)
        std::cout << "(" << p.x << "," << p.y << ") ";
    std::cout << "\n";

    return 0;
}
