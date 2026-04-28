// tests/test_json.cpp — Tests for reflect/json.hpp
//
// Compile:
//   clang++ -std=c++26 -freflection-latest -I../include test_json.cpp -o test_json

#include <reflect/json.hpp>
#include <cassert>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Test types
// ---------------------------------------------------------------------------

enum class Side { buy, sell };

struct Point { int x; int y; };

struct Trade {
    int id;
    std::string symbol;
    double price;
    int quantity;
};

struct Order {
    int id;
    std::string symbol;
    double price;
    std::optional<std::string> note;
    std::vector<int> fills;
};

struct Nested {
    Point origin;
    std::vector<Point> waypoints;
    std::string label;
};

struct WithEnum {
    std::string name;
    Side side;
};

struct WithMap {
    std::string id;
    std::map<std::string, int> scores;
};

struct WithBool {
    std::string name;
    bool active;
    bool deleted;
};

struct AllOptional {
    std::optional<int> a;
    std::optional<std::string> b;
};

// ---------------------------------------------------------------------------
// Serialization tests
// ---------------------------------------------------------------------------

void test_serialize_simple() {
    Point p{3, 7};
    auto json = reflect::to_json(p);
    assert(json == R"({"x":3,"y":7})");
    std::cout << "  serialize simple: PASS\n";
}

void test_serialize_strings() {
    Trade t{1, "AAPL", 198.5, 100};
    auto json = reflect::to_json(t);
    assert(json.find(R"("symbol":"AAPL")") != std::string::npos);
    assert(json.find(R"("id":1)") != std::string::npos);
    std::cout << "  serialize strings: PASS\n";
}

void test_serialize_optional_present() {
    Order o{1, "MSFT", 420.0, "limit order", {100, 200}};
    auto json = reflect::to_json(o);
    assert(json.find(R"("note":"limit order")") != std::string::npos);
    std::cout << "  serialize optional (present): PASS\n";
}

void test_serialize_optional_null() {
    Order o{1, "MSFT", 420.0, std::nullopt, {}};
    auto json = reflect::to_json(o);
    assert(json.find(R"("note":null)") != std::string::npos);
    std::cout << "  serialize optional (null): PASS\n";
}

void test_serialize_skip_nullopt() {
    Order o{1, "MSFT", 420.0, std::nullopt, {}};
    auto json = reflect::to_json(o, {.skip_nullopt = true});
    assert(json.find("note") == std::string::npos);
    std::cout << "  serialize skip_nullopt: PASS\n";
}

void test_serialize_vector() {
    Order o{1, "X", 100.0, std::nullopt, {50, 30, 20}};
    auto json = reflect::to_json(o);
    assert(json.find(R"("fills":[50,30,20])") != std::string::npos);
    std::cout << "  serialize vector: PASS\n";
}

void test_serialize_nested() {
    Nested n{{1, 2}, {{3, 4}, {5, 6}}, "path"};
    auto json = reflect::to_json(n);
    assert(json.find(R"("origin":{"x":1,"y":2})") != std::string::npos);
    assert(json.find(R"("waypoints":[{"x":3,"y":4},{"x":5,"y":6}])") != std::string::npos);
    std::cout << "  serialize nested: PASS\n";
}

void test_serialize_bool() {
    WithBool wb{"test", true, false};
    auto json = reflect::to_json(wb);
    assert(json.find(R"("active":true)") != std::string::npos);
    assert(json.find(R"("deleted":false)") != std::string::npos);
    std::cout << "  serialize bool: PASS\n";
}

void test_serialize_map() {
    WithMap wm{"scores", {{"alice", 100}, {"bob", 85}}};
    auto json = reflect::to_json(wm);
    assert(json.find(R"("alice":100)") != std::string::npos);
    assert(json.find(R"("bob":85)") != std::string::npos);
    std::cout << "  serialize map: PASS\n";
}

void test_serialize_enum() {
    WithEnum we{"trade1", Side::buy};
    auto json = reflect::to_json(we);
    assert(json.find(R"("side":"buy")") != std::string::npos);
    std::cout << "  serialize enum: PASS\n";
}

void test_serialize_pretty() {
    Point p{1, 2};
    auto json = reflect::to_json(p, {.indent = 2});
    assert(json.find('\n') != std::string::npos);
    assert(json.find("  ") != std::string::npos);
    std::cout << "  serialize pretty: PASS\n";
}

void test_serialize_escape() {
    struct S { std::string text; };
    S s{R"(hello "world" \n)"};
    auto json = reflect::to_json(s);
    assert(json.find(R"(\")") != std::string::npos);
    assert(json.find(R"(\\)") != std::string::npos);
    std::cout << "  serialize escape: PASS\n";
}

void test_serialize_empty_containers() {
    Order o{1, "X", 0.0, std::nullopt, {}};
    auto json = reflect::to_json(o);
    assert(json.find(R"("fills":[])") != std::string::npos);
    std::cout << "  serialize empty containers: PASS\n";
}

// ---------------------------------------------------------------------------
// Deserialization tests
// ---------------------------------------------------------------------------

void test_deserialize_simple() {
    auto p = reflect::from_json<Point>(R"({"x":3,"y":7})");
    assert(p.x == 3);
    assert(p.y == 7);
    std::cout << "  deserialize simple: PASS\n";
}

void test_deserialize_string() {
    auto t = reflect::from_json<Trade>(R"({"id":1,"symbol":"AAPL","price":198.5,"quantity":100})");
    assert(t.id == 1);
    assert(t.symbol == "AAPL");
    assert(t.price == 198.5);
    assert(t.quantity == 100);
    std::cout << "  deserialize string: PASS\n";
}

void test_deserialize_optional() {
    auto o1 = reflect::from_json<Order>(
        R"({"id":1,"symbol":"X","price":100,"note":"hi","fills":[]})");
    assert(o1.note.has_value());
    assert(*o1.note == "hi");

    auto o2 = reflect::from_json<Order>(
        R"({"id":1,"symbol":"X","price":100,"note":null,"fills":[]})");
    assert(!o2.note.has_value());

    std::cout << "  deserialize optional: PASS\n";
}

void test_deserialize_vector() {
    auto o = reflect::from_json<Order>(
        R"({"id":1,"symbol":"X","price":100,"note":null,"fills":[10,20,30]})");
    assert(o.fills.size() == 3);
    assert(o.fills[0] == 10);
    assert(o.fills[1] == 20);
    assert(o.fills[2] == 30);
    std::cout << "  deserialize vector: PASS\n";
}

void test_deserialize_nested() {
    auto n = reflect::from_json<Nested>(
        R"({"origin":{"x":1,"y":2},"waypoints":[{"x":3,"y":4}],"label":"test"})");
    assert(n.origin.x == 1);
    assert(n.origin.y == 2);
    assert(n.waypoints.size() == 1);
    assert(n.waypoints[0].x == 3);
    assert(n.label == "test");
    std::cout << "  deserialize nested: PASS\n";
}

void test_deserialize_bool() {
    auto wb = reflect::from_json<WithBool>(
        R"({"name":"test","active":true,"deleted":false})");
    assert(wb.active == true);
    assert(wb.deleted == false);
    std::cout << "  deserialize bool: PASS\n";
}

void test_deserialize_enum() {
    auto we = reflect::from_json<WithEnum>(R"({"name":"t","side":"sell"})");
    assert(we.side == Side::sell);
    std::cout << "  deserialize enum: PASS\n";
}

void test_deserialize_unknown_fields() {
    // Unknown keys should be silently ignored
    auto p = reflect::from_json<Point>(R"({"x":1,"z":99,"y":2,"extra":"ignored"})");
    assert(p.x == 1);
    assert(p.y == 2);
    std::cout << "  deserialize unknown fields: PASS\n";
}

void test_deserialize_whitespace() {
    auto p = reflect::from_json<Point>(R"(  {  "x" : 1 ,  "y" : 2  }  )");
    assert(p.x == 1);
    assert(p.y == 2);
    std::cout << "  deserialize whitespace: PASS\n";
}

void test_deserialize_escape() {
    struct S { std::string text; };
    auto s = reflect::from_json<S>(R"({"text":"hello \"world\"\ntest"})");
    assert(s.text.find('"') != std::string::npos);
    assert(s.text.find('\n') != std::string::npos);
    std::cout << "  deserialize escape: PASS\n";
}

void test_deserialize_map() {
    auto wm = reflect::from_json<WithMap>(
        R"({"id":"test","scores":{"alice":100,"bob":85}})");
    assert(wm.scores.at("alice") == 100);
    assert(wm.scores.at("bob") == 85);
    std::cout << "  deserialize map: PASS\n";
}

// ---------------------------------------------------------------------------
// Round-trip tests
// ---------------------------------------------------------------------------

void test_roundtrip_trade() {
    Trade original{42, "GOOG", 175.25, 500};
    auto json = reflect::to_json(original);
    auto restored = reflect::from_json<Trade>(json);
    assert(restored.id == original.id);
    assert(restored.symbol == original.symbol);
    assert(restored.price == original.price);
    assert(restored.quantity == original.quantity);
    std::cout << "  roundtrip trade: PASS\n";
}

void test_roundtrip_nested() {
    Nested original{{1, 2}, {{3, 4}, {5, 6}}, "route"};
    auto json = reflect::to_json(original);
    auto restored = reflect::from_json<Nested>(json);
    assert(restored.origin.x == 1);
    assert(restored.waypoints.size() == 2);
    assert(restored.label == "route");
    std::cout << "  roundtrip nested: PASS\n";
}

void test_roundtrip_all_optional() {
    AllOptional a1{42, "hello"};
    auto j1 = reflect::to_json(a1);
    auto r1 = reflect::from_json<AllOptional>(j1);
    assert(r1.a == 42);
    assert(r1.b == "hello");

    AllOptional a2{std::nullopt, std::nullopt};
    auto j2 = reflect::to_json(a2);
    auto r2 = reflect::from_json<AllOptional>(j2);
    assert(!r2.a.has_value());
    assert(!r2.b.has_value());

    std::cout << "  roundtrip all_optional: PASS\n";
}

// ---------------------------------------------------------------------------
// Surrogate pair tests
// ---------------------------------------------------------------------------

void test_surrogate_pair_emoji() {
    // U+1F600 (grinning face) encoded as surrogate pair: \uD83D\uDE00
    struct S { std::string text; };
    auto s = reflect::from_json<S>(R"({"text":"\uD83D\uDE00"})");
    // U+1F600 in UTF-8 is: F0 9F 98 80
    assert(s.text.size() == 4);
    assert(s.text[0] == '\xF0');
    assert(s.text[1] == '\x9F');
    assert(s.text[2] == '\x98');
    assert(s.text[3] == '\x80');

    // Round-trip: serialize and deserialize
    auto json = reflect::to_json(s);
    auto s2 = reflect::from_json<S>(json);
    assert(s2.text == s.text);

    std::cout << "  surrogate pair emoji: PASS\n";
}

void test_lone_surrogate_error() {
    struct S { std::string text; };

    // Lone high surrogate
    bool caught_high = false;
    try {
        reflect::from_json<S>(R"({"text":"\uD83D"})");
    } catch (reflect::json_parse_error const&) {
        caught_high = true;
    }
    assert(caught_high);

    // Lone low surrogate
    bool caught_low = false;
    try {
        reflect::from_json<S>(R"({"text":"\uDE00"})");
    } catch (reflect::json_parse_error const&) {
        caught_low = true;
    }
    assert(caught_low);

    std::cout << "  lone surrogate error: PASS\n";
}

// ---------------------------------------------------------------------------
// Error handling tests
// ---------------------------------------------------------------------------

void test_error_invalid_json() {
    bool caught = false;
    try {
        reflect::from_json<Point>("not json");
    } catch (reflect::json_parse_error const&) {
        caught = true;
    }
    assert(caught);
    std::cout << "  error invalid json: PASS\n";
}

void test_error_missing_brace() {
    bool caught = false;
    try {
        reflect::from_json<Point>(R"({"x":1,"y":2)");
    } catch (reflect::json_parse_error const&) {
        caught = true;
    }
    assert(caught);
    std::cout << "  error missing brace: PASS\n";
}

void test_error_trailing_garbage() {
    bool caught = false;
    try {
        reflect::from_json<Point>(R"({"x":1,"y":2} trailing)");
    } catch (reflect::json_parse_error const&) {
        caught = true;
    }
    assert(caught);

    caught = false;
    try {
        reflect::from_json<bool>("true false");
    } catch (reflect::json_parse_error const&) {
        caught = true;
    }
    assert(caught);

    std::cout << "  error trailing garbage: PASS\n";
}

void test_error_unescaped_control_in_string() {
    struct S { std::string text; };
    bool caught = false;
    try {
        reflect::from_json<S>("{\"text\":\"hello\nworld\"}");
    } catch (reflect::json_parse_error const&) {
        caught = true;
    }
    assert(caught);
    std::cout << "  error unescaped control in string: PASS\n";
}

void test_error_invalid_numbers() {
    bool caught = false;
    try {
        reflect::from_json<int>("01");
    } catch (reflect::json_parse_error const&) {
        caught = true;
    }
    assert(caught);

    caught = false;
    try {
        reflect::from_json<double>("1.");
    } catch (reflect::json_parse_error const&) {
        caught = true;
    }
    assert(caught);

    caught = false;
    try {
        reflect::from_json<double>("1e");
    } catch (reflect::json_parse_error const&) {
        caught = true;
    }
    assert(caught);

    std::cout << "  error invalid numbers: PASS\n";
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main() {
    std::cout << "=== test_json — serialization ===\n";
    test_serialize_simple();
    test_serialize_strings();
    test_serialize_optional_present();
    test_serialize_optional_null();
    test_serialize_skip_nullopt();
    test_serialize_vector();
    test_serialize_nested();
    test_serialize_bool();
    test_serialize_map();
    test_serialize_enum();
    test_serialize_pretty();
    test_serialize_escape();
    test_serialize_empty_containers();

    std::cout << "\n=== test_json — deserialization ===\n";
    test_deserialize_simple();
    test_deserialize_string();
    test_deserialize_optional();
    test_deserialize_vector();
    test_deserialize_nested();
    test_deserialize_bool();
    test_deserialize_enum();
    test_deserialize_unknown_fields();
    test_deserialize_whitespace();
    test_deserialize_escape();
    test_deserialize_map();

    std::cout << "\n=== test_json — round-trips ===\n";
    test_roundtrip_trade();
    test_roundtrip_nested();
    test_roundtrip_all_optional();

    std::cout << "\n=== test_json — surrogate pairs ===\n";
    test_surrogate_pair_emoji();
    test_lone_surrogate_error();

    std::cout << "\n=== test_json — error handling ===\n";
    test_error_invalid_json();
    test_error_missing_brace();
    test_error_trailing_garbage();
    test_error_unescaped_control_in_string();
    test_error_invalid_numbers();

    std::cout << "\nAll JSON tests passed!\n";
    return 0;
}
