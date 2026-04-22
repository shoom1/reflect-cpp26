// reflect/args.hpp — CLI argument parsing from struct definitions
//
// The Rust `clap derive` experience for C++26.
//
// Usage:
//   struct Args {
//       std::string input;                  // --input
//       int verbose = 0;                    // --verbose (default: 0)
//       bool debug = false;                 // --debug (flag, no value needed)
//       std::optional<std::string> output;  // --output (optional)
//       std::vector<std::string> files;     // --files a b c (multi-value)
//   };
//
//   int main(int argc, char** argv) {
//       auto args = reflect::parse_args<Args>(argc, argv);
//       // --input foo --verbose 3 --debug --files a.txt b.txt
//   }
//
//   // Or just get help text:
//   reflect::args_help<Args>("my_program");
//
// SPDX-License-Identifier: MIT

#ifndef REFLECT_ARGS_HPP
#define REFLECT_ARGS_HPP

#include "reflect/traits.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace reflect {

// ---------------------------------------------------------------------------
// Exceptions
// ---------------------------------------------------------------------------

class args_error : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class args_help_requested : public std::runtime_error {
public:
    args_help_requested(std::string help_text)
        : std::runtime_error("help requested"), help_text_(std::move(help_text)) {}
    std::string const& help_text() const noexcept { return help_text_; }
private:
    std::string help_text_;
};

namespace args_detail {

    consteval auto args_members_of(std::meta::info t) {
        return std::meta::nonstatic_data_members_of(
            t, std::meta::access_context::current());
    }

    template <reflectable T>
    consteval std::size_t args_field_count() {
        return args_members_of(^^T).size();
    }

    // -----------------------------------------------------------------------
    // Type traits
    // -----------------------------------------------------------------------

    template <typename T>
    concept is_bool = std::same_as<std::remove_cvref_t<T>, bool>;

    template <typename T>
    concept is_vector = requires(T t) {
        typename T::value_type;
        { t.push_back(std::declval<typename T::value_type>()) };
    } && !reflect::is_string_like<T>;

    // Field name "my_field" → "--my-field"
    inline std::string to_flag_name(std::string_view field_name) {
        std::string flag = "--";
        for (char c : field_name) {
            flag += (c == '_') ? '-' : c;
        }
        return flag;
    }

    // Parse a single value from string
    template <typename T>
    T parse_single(std::string_view str, std::string_view flag_name) {
        if constexpr (std::same_as<T, std::string>) {
            return std::string(str);
        } else if constexpr (std::is_integral_v<T>) {
            T val{};
            auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), val);
            if (ec != std::errc{} || ptr != str.data() + str.size())
                throw args_error("invalid integer for " + std::string(flag_name) +
                                 ": '" + std::string(str) + "'");
            return val;
        } else if constexpr (std::is_floating_point_v<T>) {
            T val{};
            auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), val);
            if (ec != std::errc{} || ptr != str.data() + str.size())
                throw args_error("invalid number for " + std::string(flag_name) +
                                 ": '" + std::string(str) + "'");
            return val;
        } else {
            static_assert(sizeof(T) == 0, "Unsupported argument type");
        }
    }

    template <typename T>
    constexpr std::string_view type_label() {
        if constexpr (std::same_as<T, std::string>)     return "STRING";
        else if constexpr (std::same_as<T, bool>)       return "BOOL";
        else if constexpr (std::is_integral_v<T>)        return "INT";
        else if constexpr (std::is_floating_point_v<T>)  return "FLOAT";
        else return "VALUE";
    }

} // namespace args_detail

// ---------------------------------------------------------------------------
// args_help<T>(program_name) — generate help text from a struct
// ---------------------------------------------------------------------------

template <reflectable T>
std::string args_help(std::string_view program_name = "program") {
    std::ostringstream os;
    os << "Usage: " << program_name << " [OPTIONS]\n\nOptions:\n";

    template for (constexpr auto member : args_detail::args_members_of(^^T)) {
        using MType = typename[:std::meta::type_of(member):];
        constexpr auto name = std::meta::identifier_of(member);
        auto flag = args_detail::to_flag_name(name);

        os << "  " << flag;

        if constexpr (args_detail::is_bool<MType>) {
            os << std::string(24 - flag.size() - 2, ' ');
            os << "(flag)";
        } else if constexpr (reflect::is_optional<MType>) {
            using Inner = typename MType::value_type;
            auto label = args_detail::type_label<Inner>();
            os << " <" << label << ">";
            auto pad = 24 - flag.size() - 3 - label.size() - 2;
            if (pad > 0 && pad < 30) os << std::string(pad, ' ');
            os << "(optional)";
        } else if constexpr (args_detail::is_vector<MType>) {
            using Inner = typename MType::value_type;
            auto label = args_detail::type_label<Inner>();
            os << " <" << label << ">...";
            auto pad = 24 - flag.size() - 3 - label.size() - 5;
            if (pad > 0 && pad < 30) os << std::string(pad, ' ');
            os << "(multi-value)";
        } else {
            auto label = args_detail::type_label<MType>();
            os << " <" << label << ">";
            auto pad = 24 - flag.size() - 3 - label.size() - 1;
            if (pad > 0 && pad < 30) os << std::string(pad, ' ');
        }

        os << "\n";
    }

    os << "  --help                  Show this help message\n";
    return os.str();
}

// ---------------------------------------------------------------------------
// parse_args<T>(argc, argv) — parse command-line arguments into a struct
// ---------------------------------------------------------------------------

template <reflectable T>
T parse_args(int argc, char const* const* argv) {
    T result{};

    // Build the argument list (skip program name)
    std::vector<std::string_view> args;
    for (int i = 1; i < argc; ++i)
        args.emplace_back(argv[i]);

    // Check for --help
    for (auto const& arg : args) {
        if (arg == "--help" || arg == "-h") {
            std::string prog = (argc > 0) ? argv[0] : "program";
            throw args_help_requested(args_help<T>(prog));
        }
    }

    // Track which fields were explicitly set. Use std::array sized by
    // the consteval field count — no VLA, no GNU extensions.
    constexpr auto field_count = args_detail::args_field_count<T>();
    std::array<bool, field_count> field_set{};

    std::size_t i = 0;
    while (i < args.size()) {
        auto arg = args[i];

        if (!arg.starts_with("--")) {
            throw args_error("unexpected positional argument: '" + std::string(arg) + "'");
        }

        bool matched = false;
        std::size_t field_idx = 0;

        template for (constexpr auto member : args_detail::args_members_of(^^T)) {
            using MType = typename[:std::meta::type_of(member):];
            constexpr auto name = std::meta::identifier_of(member);
            auto flag = args_detail::to_flag_name(name);

            if (!matched && arg == flag) {
                matched = true;
                field_set[field_idx] = true;

                if constexpr (args_detail::is_bool<MType>) {
                    result.[:member:] = true;
                    ++i;
                } else if constexpr (args_detail::is_vector<MType>) {
                    ++i;
                    using Inner = typename MType::value_type;
                    while (i < args.size() && !args[i].starts_with("--")) {
                        result.[:member:].push_back(
                            args_detail::parse_single<Inner>(args[i], flag));
                        ++i;
                    }
                } else if constexpr (reflect::is_optional<MType>) {
                    ++i;
                    if (i >= args.size())
                        throw args_error("missing value for " + flag);
                    using Inner = typename MType::value_type;
                    result.[:member:] = args_detail::parse_single<Inner>(args[i], flag);
                    ++i;
                } else {
                    ++i;
                    if (i >= args.size())
                        throw args_error("missing value for " + flag);
                    result.[:member:] = args_detail::parse_single<MType>(args[i], flag);
                    ++i;
                }
            }
            ++field_idx;
        }

        if (!matched) {
            throw args_error("unknown argument: '" + std::string(arg) + "'");
        }
    }

    // Validate required fields: non-bool, non-optional, non-vector fields must be set.
    {
        std::size_t idx = 0;
        template for (constexpr auto member : args_detail::args_members_of(^^T)) {
            using MType = typename[:std::meta::type_of(member):];
            if constexpr (!args_detail::is_bool<MType> &&
                          !reflect::is_optional<MType> &&
                          !args_detail::is_vector<MType>) {
                if (!field_set[idx]) {
                    constexpr auto name = std::meta::identifier_of(member);
                    throw args_error("missing required argument: " +
                                     args_detail::to_flag_name(name));
                }
            }
            ++idx;
        }
    }

    return result;
}

// ---------------------------------------------------------------------------
// Overload taking std::vector<std::string_view> for testing
// ---------------------------------------------------------------------------

template <reflectable T>
T parse_args(std::vector<std::string_view> const& argv) {
    std::vector<char const*> ptrs;
    ptrs.push_back("program");
    for (auto& s : argv) ptrs.push_back(s.data());
    return parse_args<T>(static_cast<int>(ptrs.size()), ptrs.data());
}

} // namespace reflect

#endif // REFLECT_ARGS_HPP
