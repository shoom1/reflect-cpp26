// examples/args.cpp — CLI argument parsing from a struct (Rust-clap-derive style)

#include <reflect/args.hpp>

#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct CliArgs {
    std::string input;
    int verbose = 0;
    bool debug  = false;
    std::optional<std::string> output;
    std::vector<std::string> files;
};

int main() {
    // Hand-built argv to keep the example self-contained.
    // Field name `input` becomes flag `--input`, `verbose` → `--verbose`, etc.
    auto args = reflect::parse_args<CliArgs>(std::vector<std::string_view>{
        "--input",   "data.csv",
        "--verbose", "2",
        "--debug",
        "--output",  "result.json",
        "--files",   "a.txt", "b.txt", "c.txt",
    });

    std::cout << "input:   " << args.input   << "\n";
    std::cout << "verbose: " << args.verbose << "\n";
    std::cout << "debug:   " << std::boolalpha << args.debug << "\n";
    std::cout << "output:  " << args.output.value_or("(none)") << "\n";
    std::cout << "files:   ";
    for (auto const& f : args.files) std::cout << f << " ";
    std::cout << "\n\n";

    // Auto-generated --help text from the same struct
    std::cout << reflect::args_help<CliArgs>("myprog");

    return 0;
}
