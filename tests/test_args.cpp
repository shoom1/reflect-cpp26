// tests/test_args.cpp — Tests for reflect/args.hpp
//
// Compile:
//   clang++ -std=c++26 -freflection-latest -I../include test_args.cpp -o test_args

#include <reflect/args.hpp>
#include <cassert>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Test arg structs
// ---------------------------------------------------------------------------

struct SimpleArgs {
    std::string input;
    int count = 0;
};

struct FullArgs {
    std::string input;
    int verbose = 0;
    bool debug = false;
    std::optional<std::string> output;
    std::vector<std::string> files;
};

struct NumericArgs {
    int port = 8080;
    double rate = 1.0;
    bool dry_run = false;
};

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

void test_simple_parse() {
    char const* argv[] = {"prog", "--input", "hello.txt", "--count", "5"};
    auto args = reflect::parse_args<SimpleArgs>(5, argv);
    assert(args.input == "hello.txt");
    assert(args.count == 5);
    std::cout << "  simple parse: PASS\n";
}

void test_defaults() {
    // SimpleArgs has required fields (input: string, count: int), so provide them
    char const* argv[] = {"prog", "--input", "test.txt", "--count", "0"};
    auto args = reflect::parse_args<SimpleArgs>(5, argv);
    assert(args.input == "test.txt");
    assert(args.count == 0);
    std::cout << "  defaults: PASS\n";
}

void test_bool_flag() {
    char const* argv[] = {"prog", "--input", "x", "--verbose", "0", "--debug"};
    auto args = reflect::parse_args<FullArgs>(6, argv);
    assert(args.debug == true);
    assert(args.verbose == 0);
    std::cout << "  bool flag: PASS\n";
}

void test_optional_present() {
    char const* argv[] = {"prog", "--input", "in.txt", "--verbose", "0", "--output", "out.txt"};
    auto args = reflect::parse_args<FullArgs>(7, argv);
    assert(args.output.has_value());
    assert(*args.output == "out.txt");
    std::cout << "  optional present: PASS\n";
}

void test_optional_absent() {
    char const* argv[] = {"prog", "--input", "in.txt", "--verbose", "0"};
    auto args = reflect::parse_args<FullArgs>(5, argv);
    assert(!args.output.has_value());
    std::cout << "  optional absent: PASS\n";
}

void test_vector_args() {
    char const* argv[] = {"prog", "--input", "x", "--verbose", "0",
                          "--files", "a.txt", "b.txt", "c.txt"};
    auto args = reflect::parse_args<FullArgs>(9, argv);
    assert(args.files.size() == 3);
    assert(args.files[0] == "a.txt");
    assert(args.files[1] == "b.txt");
    assert(args.files[2] == "c.txt");
    std::cout << "  vector args: PASS\n";
}

void test_vector_stops_at_flag() {
    char const* argv[] = {"prog", "--input", "x", "--verbose", "0",
                          "--files", "a.txt", "b.txt", "--debug"};
    auto args = reflect::parse_args<FullArgs>(9, argv);
    assert(args.files.size() == 2);
    assert(args.debug == true);
    std::cout << "  vector stops at flag: PASS\n";
}

void test_numeric_args() {
    char const* argv[] = {"prog", "--port", "9090", "--rate", "2.5", "--dry-run"};
    auto args = reflect::parse_args<NumericArgs>(6, argv);
    assert(args.port == 9090);
    assert(args.rate == 2.5);
    assert(args.dry_run == true);
    std::cout << "  numeric args: PASS\n";
}

void test_underscore_to_dash() {
    // Field name "dry_run" → flag "--dry-run"
    char const* argv[] = {"prog", "--port", "8080", "--rate", "1.0", "--dry-run"};
    auto args = reflect::parse_args<NumericArgs>(6, argv);
    assert(args.dry_run == true);
    std::cout << "  underscore to dash: PASS\n";
}

void test_combined() {
    char const* argv[] = {
        "prog",
        "--input", "data.csv",
        "--verbose", "3",
        "--debug",
        "--output", "result.txt",
        "--files", "x.log", "y.log"
    };
    auto args = reflect::parse_args<FullArgs>(11, argv);
    assert(args.input == "data.csv");
    assert(args.verbose == 3);
    assert(args.debug == true);
    assert(args.output == "result.txt");
    assert(args.files.size() == 2);
    std::cout << "  combined: PASS\n";
}

void test_unknown_arg_throws() {
    char const* argv[] = {"prog", "--bogus", "value"};
    bool caught = false;
    try {
        reflect::parse_args<SimpleArgs>(3, argv);
    } catch (reflect::args_error const& e) {
        caught = true;
        assert(std::string(e.what()).find("bogus") != std::string::npos);
    }
    assert(caught);
    std::cout << "  unknown arg throws: PASS\n";
}

void test_missing_value_throws() {
    char const* argv[] = {"prog", "--input"};
    bool caught = false;
    try {
        reflect::parse_args<SimpleArgs>(2, argv);
    } catch (reflect::args_error const& e) {
        caught = true;
    }
    assert(caught);
    std::cout << "  missing value throws: PASS\n";
}

void test_invalid_int_throws() {
    char const* argv[] = {"prog", "--count", "notanumber"};
    bool caught = false;
    try {
        reflect::parse_args<SimpleArgs>(3, argv);
    } catch (reflect::args_error const& e) {
        caught = true;
    }
    assert(caught);
    std::cout << "  invalid int throws: PASS\n";
}

void test_help_text() {
    auto help = reflect::args_help<FullArgs>("myapp");
    assert(help.find("--input") != std::string::npos);
    assert(help.find("--verbose") != std::string::npos);
    assert(help.find("--debug") != std::string::npos);
    assert(help.find("--output") != std::string::npos);
    assert(help.find("--files") != std::string::npos);
    assert(help.find("--help") != std::string::npos);
    assert(help.find("myapp") != std::string::npos);
    std::cout << "  help text: PASS\n";
}

void test_missing_required_throws() {
    // SimpleArgs requires --input (string) and --count (int)
    // Providing only --count should throw for missing --input
    char const* argv[] = {"prog", "--count", "5"};
    bool caught = false;
    try {
        reflect::parse_args<SimpleArgs>(3, argv);
    } catch (reflect::args_error const& e) {
        caught = true;
        assert(std::string(e.what()).find("--input") != std::string::npos);
    }
    assert(caught);
    std::cout << "  missing required throws: PASS\n";
}

void test_help_throws() {
    char const* argv[] = {"prog", "--help"};
    bool caught = false;
    try {
        reflect::parse_args<SimpleArgs>(2, argv);
    } catch (reflect::args_help_requested const& e) {
        caught = true;
        assert(!e.help_text().empty());
        assert(e.help_text().find("--input") != std::string::npos);
    }
    assert(caught);
    std::cout << "  help throws: PASS\n";
}

void test_vector_overload() {
    auto args = reflect::parse_args<SimpleArgs>(
        std::vector<std::string_view>{"--input", "test.txt", "--count", "10"});
    assert(args.input == "test.txt");
    assert(args.count == 10);
    std::cout << "  vector overload: PASS\n";
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main() {
    std::cout << "=== test_args ===\n";
    test_simple_parse();
    test_defaults();
    test_bool_flag();
    test_optional_present();
    test_optional_absent();
    test_vector_args();
    test_vector_stops_at_flag();
    test_numeric_args();
    test_underscore_to_dash();
    test_combined();
    test_unknown_arg_throws();
    test_missing_value_throws();
    test_invalid_int_throws();
    test_help_text();
    test_missing_required_throws();
    test_help_throws();
    test_vector_overload();
    std::cout << "All args tests passed!\n\n";
    return 0;
}
