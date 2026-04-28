// reflect/args.hpp — CLI argument parsing from struct definitions
//
// The Rust `clap derive` experience for C++26.
//
// Usage:
//   struct Args {
//       std::string input;                  // --input <STRING> (required)
//       int verbose = 0;                    // --verbose <INT> (required)
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
        return std::define_static_array(
            std::meta::nonstatic_data_members_of(
                t, std::meta::access_context::current()));
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
        } else if constexpr (std::same_as<T, bool>) {
            if (str == "true" || str == "1" || str == "yes" || str == "on")
                return true;
            if (str == "false" || str == "0" || str == "no" || str == "off")
                return false;
            throw args_error("invalid boolean for " + std::string(flag_name) +
                             ": '" + std::string(str) + "'");
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

    inline void append_help_padding(std::ostringstream& os, std::size_t used, std::size_t width = 24) {
        if (used < width)
            os << std::string(width - used, ' ');
    }

} // namespace args_detail

// ---------------------------------------------------------------------------
// args_help<T>(program_name) — generate help text from a struct
// ---------------------------------------------------------------------------

// Marked constexpr so the `template for` over args_members_of(^^T)
// accepts the consteval-helper-returned std::vector<info> as its range.
// Other modules (compare, visit, json, …) follow the same pattern.
template <reflectable T>
constexpr std::string args_help(std::string_view program_name = "program") {
    std::ostringstream os;
    os << "Usage: " << program_name << " [OPTIONS]\n\nOptions:\n";

    template for (constexpr auto member : args_detail::args_members_of(^^T)) {
        using MType = typename[:std::meta::type_of(member):];
        constexpr auto name = std::meta::identifier_of(member);
        auto flag = args_detail::to_flag_name(name);

        os << "  " << flag;

        if constexpr (args_detail::is_bool<MType>) {
            args_detail::append_help_padding(os, 2 + flag.size());
            os << "(flag)";
        } else if constexpr (reflect::is_optional<MType>) {
            using Inner = typename MType::value_type;
            auto label = args_detail::type_label<Inner>();
            os << " <" << label << ">";
            args_detail::append_help_padding(os, 2 + flag.size() + 3 + label.size());
            os << "(optional)";
        } else if constexpr (args_detail::is_vector<MType>) {
            using Inner = typename MType::value_type;
            auto label = args_detail::type_label<Inner>();
            os << " <" << label << ">...";
            args_detail::append_help_padding(os, 2 + flag.size() + 6 + label.size());
            os << "(multi-value)";
        } else {
            auto label = args_detail::type_label<MType>();
            os << " <" << label << ">";
            args_detail::append_help_padding(os, 2 + flag.size() + 3 + label.size());
            os << "(required)";
        }

        os << "\n";
    }

    os << "  --help                  Show this help message\n";
    return os.str();
}

// ---------------------------------------------------------------------------
// parse_args<T>(argc, argv) — parse command-line arguments into a struct
// ---------------------------------------------------------------------------

namespace args_detail {

template <reflectable T>
constexpr T parse_args_views(std::vector<std::string_view> const& args,
                             std::string_view program_name = "program") {
    T result{};

    // Check for --help
    for (auto const& arg : args) {
        if (arg == "--help" || arg == "-h") {
            throw args_help_requested(args_help<T>(program_name));
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

} // namespace args_detail

template <reflectable T>
constexpr T parse_args(int argc, char const* const* argv) {
    std::vector<std::string_view> args;
    for (int i = 1; i < argc; ++i)
        args.emplace_back(argv[i]);

    std::string_view program_name = "program";
    if (argc > 0 && argv[0] != nullptr)
        program_name = argv[0];

    return args_detail::parse_args_views<T>(args, program_name);
}

// ---------------------------------------------------------------------------
// Overload taking std::vector<std::string_view> for testing
// ---------------------------------------------------------------------------

template <reflectable T>
constexpr T parse_args(std::vector<std::string_view> const& argv) {
    return args_detail::parse_args_views<T>(argv);
}

} // namespace reflect

#endif // REFLECT_ARGS_HPP
