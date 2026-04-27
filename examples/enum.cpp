// examples/enum.cpp — enum reflection (replaces magic_enum)

#include <reflect/enum.hpp>

#include <iostream>
#include <string>

enum class Color { red, green, blue };

enum class Perm : unsigned {
    none  = 0,
    read  = 1,
    write = 2,
    exec  = 4,
};

// Opt in to bitwise operators for Perm:
template <>
struct reflect::enable_bitmask_operators<Perm> : std::true_type {};

int main() {
    // to_string / from_string
    std::cout << "enum_to_string(Color::green) = \""
              << reflect::enum_to_string(Color::green) << "\"\n";

    auto blue = reflect::enum_from_string<Color>("blue");
    std::cout << "enum_from_string<Color>(\"blue\") = "
              << (blue ? reflect::enum_to_string(*blue) : "(nullopt)") << "\n";

    // count, names, values
    std::cout << "enum_count<Color>() = " << reflect::enum_count<Color>() << "\n";
    std::cout << "enum_names<Color>(): ";
    for (auto n : reflect::enum_names<Color>()) std::cout << n << " ";
    std::cout << "\n";

    // contains / cast / index
    std::cout << "enum_contains<Color>(1) = " << std::boolalpha
              << reflect::enum_contains<Color>(1) << "\n";
    std::cout << "enum_contains<Color>(99) = "
              << reflect::enum_contains<Color>(99) << "\n";

    auto cast = reflect::enum_cast<Color>(2);
    std::cout << "enum_cast<Color>(2) = "
              << (cast ? reflect::enum_to_string(*cast) : "(nullopt)") << "\n";

    auto idx = reflect::enum_index(Color::blue);
    std::cout << "enum_index(Color::blue) = " << *idx << "\n";

    // Flags / bitmask
    auto perms = Perm::read | Perm::exec;
    std::cout << "Perm::read|exec → \""
              << reflect::flags_to_string(perms) << "\"\n";
    auto rw = reflect::flags_from_string<Perm>("read|write");
    std::cout << "\"read|write\" → \""
              << reflect::flags_to_string(*rw) << "\"\n";

    return 0;
}
