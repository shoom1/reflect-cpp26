// examples/json.cpp — zero-boilerplate JSON ser/deser (replaces nlohmann/json)

#include <reflect/json.hpp>

#include <iostream>
#include <optional>
#include <string>
#include <vector>

enum class Side { buy, sell };

struct Trade {
    int id;
    std::string symbol;
    double price;
    int quantity;
    Side side;
    std::vector<std::string> tags;
    std::optional<std::string> note;
};

int main() {
    Trade t{42, "AAPL", 198.50, 100, Side::buy, {"tech", "earnings"}, std::nullopt};

    // Compact (default)
    auto compact = reflect::to_json(t);
    std::cout << "compact:\n  " << compact << "\n\n";

    // Pretty-printed
    std::cout << "pretty:\n" << reflect::to_json(t, {.indent = 2}) << "\n\n";

    // skip_nullopt — drop fields whose value is std::nullopt
    std::cout << "skip_nullopt:\n  "
              << reflect::to_json(t, {.skip_nullopt = true}) << "\n\n";

    // Round-trip: serialize → deserialize → struct
    auto t2 = reflect::from_json<Trade>(compact);
    std::cout << "round-trip symbol=" << t2.symbol
              << " price=" << t2.price
              << " tags=[";
    for (auto const& tag : t2.tags) std::cout << tag << " ";
    std::cout << "]\n";

    return 0;
}
