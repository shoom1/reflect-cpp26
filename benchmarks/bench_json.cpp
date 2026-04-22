// benchmarks/bench_json.cpp — JSON serialization benchmarks
//
// Compare reflect::to_json / from_json performance.
// Requires a C++26 reflection-capable compiler.
//
// Compile:
//   clang++ -std=c++26 -freflection-latest -O2 -I../include bench_json.cpp -o bench_json
//
// For comparison with nlohmann/json, install it and uncomment the #include.

#include <reflect/json.hpp>
#include <reflect/enum.hpp>

#include <chrono>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

// Uncomment to include nlohmann/json comparison:
// #include <nlohmann/json.hpp>

// ---------------------------------------------------------------------------
// Test types
// ---------------------------------------------------------------------------

enum class Side { buy, sell };

struct SmallStruct {
    int x;
    int y;
};

struct MediumStruct {
    int id;
    std::string symbol;
    double price;
    int quantity;
    Side side;
    std::vector<int> fills;
};

struct LargeStruct {
    int id;
    std::string name;
    std::string email;
    int age;
    double balance;
    bool active;
    std::vector<std::string> tags;
    std::optional<std::string> notes;
    std::vector<int> scores;
    std::string address;
};

struct NestedStruct {
    SmallStruct origin;
    std::vector<SmallStruct> waypoints;
    std::string label;
};

// ---------------------------------------------------------------------------
// Benchmark harness
// ---------------------------------------------------------------------------

template <typename F>
double bench(F&& func, std::size_t iterations) {
    // Warmup
    for (std::size_t i = 0; i < iterations / 10; ++i)
        func();

    auto start = std::chrono::high_resolution_clock::now();
    for (std::size_t i = 0; i < iterations; ++i)
        func();
    auto end = std::chrono::high_resolution_clock::now();

    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    return static_cast<double>(ns) / static_cast<double>(iterations);
}

void print_result(std::string_view label, double ns_per_op, std::size_t ops) {
    double mops = 1e9 / ns_per_op / 1e6;
    std::cout << "  " << std::setw(35) << std::left << label
              << std::setw(10) << std::right << std::fixed << std::setprecision(1)
              << ns_per_op << " ns/op"
              << "  (" << std::setprecision(2) << mops << " Mops/s)"
              << "  [" << ops << " iterations]\n";
}

// ---------------------------------------------------------------------------
// Benchmarks
// ---------------------------------------------------------------------------

constexpr std::size_t SMALL_ITERS  = 1'000'000;
constexpr std::size_t MEDIUM_ITERS = 500'000;
constexpr std::size_t LARGE_ITERS  = 200'000;

void bench_serialize() {
    std::cout << "\n--- Serialization (to_json) ---\n";

    SmallStruct small{42, 99};
    auto ns = bench([&] {
        auto s = reflect::to_json(small);
        (void)s;
    }, SMALL_ITERS);
    print_result("SmallStruct (2 ints)", ns, SMALL_ITERS);

    MediumStruct med{1, "AAPL", 198.5, 100, Side::buy, {50, 30, 20}};
    ns = bench([&] {
        auto s = reflect::to_json(med);
        (void)s;
    }, MEDIUM_ITERS);
    print_result("MediumStruct (6 fields, vector)", ns, MEDIUM_ITERS);

    LargeStruct large{
        1, "John Doe", "john@example.com", 35, 10000.50, true,
        {"admin", "user", "premium"}, "Some notes here",
        {95, 87, 92, 88, 91}, "123 Main St"
    };
    ns = bench([&] {
        auto s = reflect::to_json(large);
        (void)s;
    }, LARGE_ITERS);
    print_result("LargeStruct (10 fields, vectors)", ns, LARGE_ITERS);

    NestedStruct nested{
        {0, 0}, {{1, 2}, {3, 4}, {5, 6}, {7, 8}}, "path"
    };
    ns = bench([&] {
        auto s = reflect::to_json(nested);
        (void)s;
    }, MEDIUM_ITERS);
    print_result("NestedStruct (nested + vector)", ns, MEDIUM_ITERS);
}

void bench_deserialize() {
    std::cout << "\n--- Deserialization (from_json) ---\n";

    std::string small_json = R"({"x":42,"y":99})";
    auto ns = bench([&] {
        auto s = reflect::from_json<SmallStruct>(small_json);
        (void)s;
    }, SMALL_ITERS);
    print_result("SmallStruct (2 ints)", ns, SMALL_ITERS);

    std::string med_json = R"({"id":1,"symbol":"AAPL","price":198.5,"quantity":100,"side":"buy","fills":[50,30,20]})";
    ns = bench([&] {
        auto s = reflect::from_json<MediumStruct>(med_json);
        (void)s;
    }, MEDIUM_ITERS);
    print_result("MediumStruct (6 fields, vector)", ns, MEDIUM_ITERS);

    std::string large_json = R"({"id":1,"name":"John Doe","email":"john@example.com","age":35,"balance":10000.5,"active":true,"tags":["admin","user","premium"],"notes":"Some notes","scores":[95,87,92,88,91],"address":"123 Main St"})";
    ns = bench([&] {
        auto s = reflect::from_json<LargeStruct>(large_json);
        (void)s;
    }, LARGE_ITERS);
    print_result("LargeStruct (10 fields, vectors)", ns, LARGE_ITERS);
}

void bench_roundtrip() {
    std::cout << "\n--- Round-trip (serialize + deserialize) ---\n";

    MediumStruct med{1, "AAPL", 198.5, 100, Side::buy, {50, 30, 20}};
    auto ns = bench([&] {
        auto json = reflect::to_json(med);
        auto restored = reflect::from_json<MediumStruct>(json);
        (void)restored;
    }, MEDIUM_ITERS);
    print_result("MediumStruct round-trip", ns, MEDIUM_ITERS);
}

void bench_enum() {
    std::cout << "\n--- Enum operations ---\n";

    auto ns = bench([&] {
        auto s = reflect::enum_to_string(Side::buy);
        (void)s;
    }, SMALL_ITERS);
    print_result("enum_to_string", ns, SMALL_ITERS);

    ns = bench([&] {
        auto v = reflect::enum_from_string<Side>("sell");
        (void)v;
    }, SMALL_ITERS);
    print_result("enum_from_string", ns, SMALL_ITERS);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main() {
    std::cout << "=== reflect benchmark suite ===\n";
    std::cout << "Compiler: " << __VERSION__ << "\n";
    std::cout << "Optimization: "
#ifdef __OPTIMIZE__
              << "enabled (-O2/-O3)"
#else
              << "DISABLED (compile with -O2 for meaningful results!)"
#endif
              << "\n";

    bench_serialize();
    bench_deserialize();
    bench_roundtrip();
    bench_enum();

    std::cout << "\nDone.\n";
    return 0;
}
