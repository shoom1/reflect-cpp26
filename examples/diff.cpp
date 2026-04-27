// examples/diff.cpp — structural diff between two struct instances

#include <reflect/diff.hpp>

#include <iostream>
#include <string>

struct Config {
    std::string host;
    int port;
    bool tls;
};

int main() {
    Config a{"localhost", 8080, false};
    Config b{"localhost", 9090, true};

    // Quick predicates
    std::cout << "has_changes(a, b)  = " << std::boolalpha
              << reflect::has_changes(a, b) << "\n";
    std::cout << "change_count(a, b) = "
              << reflect::change_count(a, b) << "\n";

    // Human-readable summary
    std::cout << "diff_summary:      "
              << reflect::diff_summary(a, b) << "\n";

    // Just the names
    std::cout << "changed fields:    ";
    for (auto name : reflect::changed_field_names(a, b))
        std::cout << name << " ";
    std::cout << "\n";

    // Structured diff entries
    std::cout << "diff entries:\n";
    for (auto const& e : reflect::diff(a, b))
        std::cout << "  [" << e.index << "] " << e.name
                  << " changed=" << e.changed << "\n";

    // apply_changes: copy every changed field from `b` into `target`
    Config target = a;
    reflect::apply_changes(target, b);
    std::cout << "after apply_changes(target, b): "
              << "port=" << target.port
              << " tls=" << target.tls << "\n";

    return 0;
}
