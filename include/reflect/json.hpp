// reflect/json.hpp — Zero-boilerplate JSON serialization via C++26 reflection
//
// No macros. No registration. Just works.
//
// Usage:
//   struct Trade {
//       int id;
//       std::string symbol;
//       double price;
//       std::optional<std::string> note;
//   };
//
//   Trade t{42, "AAPL", 198.5, "earnings"};
//
//   std::string json = reflect::to_json(t);
//   // → {"id":42,"symbol":"AAPL","price":198.5,"note":"earnings"}
//
//   auto t2 = reflect::from_json<Trade>(json);
//
// Supports: nested structs, vectors, maps, optionals, enums, booleans, nullptr
//
// Pretty-printing:
//   reflect::to_json(t, reflect::json_opts{.indent = 2})
//
// SPDX-License-Identifier: MIT

#ifndef REFLECT_JSON_HPP
#define REFLECT_JSON_HPP

#include "reflect/traits.hpp"
#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#if __has_include("reflect/enum.hpp")
#include "reflect/enum.hpp"
#define REFLECT_JSON_HAS_ENUM 1
#else
#define REFLECT_JSON_HAS_ENUM 0
#endif

namespace reflect {

// ---------------------------------------------------------------------------
// Options
// ---------------------------------------------------------------------------

struct json_opts {
    int indent = -1;            // -1 = compact, 0+ = pretty with that many spaces
    bool skip_nullopt = false;  // if true, omit fields with std::nullopt
};

class json_parse_error : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// =========================================================================
// SERIALIZATION (struct → JSON string)
// =========================================================================

namespace json_detail {

    consteval auto json_members_of(std::meta::info t) {
        return std::define_static_array(
            std::meta::nonstatic_data_members_of(
                t, std::meta::access_context::current()));
    }

    // ---- type categories for JSON dispatch ----
    template <typename T>
    concept json_map = requires(T t) {
        typename T::key_type;
        typename T::mapped_type;
        { t.begin() } -> std::input_iterator;
    } && reflect::is_string_like<typename T::key_type>;

    template <typename T>
    concept json_array = reflect::is_range<T> && !json_map<T>;

    template <typename T>
    concept json_struct = reflect::reflectable<T>
        && !json_map<T> && !json_array<T>;

    // Escape a JSON string
    inline std::string escape(std::string_view s) {
        std::string out;
        out.reserve(s.size() + 2);
        out += '"';
        for (char c : s) {
            switch (c) {
                case '"':  out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\b': out += "\\b";  break;
                case '\f': out += "\\f";  break;
                case '\n': out += "\\n";  break;
                case '\r': out += "\\r";  break;
                case '\t': out += "\\t";  break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20) {
                        char buf[8];
                        std::snprintf(buf, sizeof(buf), "\\u%04x",
                                      static_cast<unsigned>(static_cast<unsigned char>(c)));
                        out += buf;
                    } else {
                        out += c;
                    }
            }
        }
        out += '"';
        return out;
    }

    inline void write_indent(std::string& out, int depth, int indent) {
        if (indent < 0) return;
        out += '\n';
        out.append(static_cast<std::size_t>(depth * indent), ' ');
    }

    // Forward declaration of the dispatch primary. Required so that the
    // array / map / struct overloads below can name-resolve `serialize`
    // for their element types at template definition time.
    //
    // Every templated overload below shares this exact parameter shape
    // (T const&); they differ only by their requires-clauses, which lets
    // partial ordering pick the constrained overload over this primary.
    // Without uniform parameter types, partial ordering can't compare
    // (T const&) against (T) and the call becomes ambiguous.
    template <typename T>
    void serialize(std::string& out, T const& value, json_opts const& opts, int depth);

    // --- Null ---
    inline void serialize(std::string& out, std::nullptr_t, json_opts const&, int) {
        out += "null";
    }

    // --- Booleans ---
    inline void serialize(std::string& out, bool value, json_opts const&, int) {
        out += value ? "true" : "false";
    }

    // --- Integers ---
    template <typename T>
        requires (std::is_integral_v<T> && !std::is_same_v<T, bool>)
    void serialize(std::string& out, T const& value, json_opts const&, int) {
        char buf[64];
        auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), value);
        out.append(buf, static_cast<std::size_t>(ptr - buf));
    }

    // --- Floating point — guard NaN/Inf which aren't valid JSON ---
    template <typename T>
        requires std::is_floating_point_v<T>
    void serialize(std::string& out, T const& value, json_opts const&, int) {
        if (!std::isfinite(value))
            throw json_parse_error(
                "cannot serialize non-finite floating-point value as JSON "
                "(NaN/Inf are not allowed by RFC 8259)");
        char buf[64];
        auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), value);
        out.append(buf, static_cast<std::size_t>(ptr - buf));
    }

    // --- Strings ---
    template <reflect::is_string_like T>
    void serialize(std::string& out, T const& value, json_opts const&, int) {
        out += escape(std::string_view(value));
    }

    inline void serialize(std::string& out, char const* value, json_opts const&, int) {
        if (value) out += escape(value);
        else       out += "null";
    }

    // --- Enums ---
    template <typename T>
        requires std::is_enum_v<T>
    void serialize(std::string& out, T const& value, json_opts const& opts, int depth) {
#if REFLECT_JSON_HAS_ENUM
        auto name = reflect::enum_to_string(value);
        if (!name.empty()) {
            out += escape(name);
            return;
        }
#endif
        // Fallback: numeric
        serialize(out, static_cast<std::underlying_type_t<T>>(value), opts, depth);
    }

    // --- Optionals ---
    template <reflect::is_optional T>
    void serialize(std::string& out, T const& value, json_opts const& opts, int depth) {
        if (value.has_value())
            serialize(out, *value, opts, depth);
        else
            out += "null";
    }

    // --- Arrays / Vectors / Ranges ---
    template <json_array T>
    void serialize(std::string& out, T const& range, json_opts const& opts, int depth) {
        out += '[';
        bool first = true;
        for (auto const& elem : range) {
            if (!first) out += ',';
            first = false;
            if (opts.indent >= 0) write_indent(out, depth + 1, opts.indent);
            serialize(out, elem, opts, depth + 1);
        }
        if (!first && opts.indent >= 0)
            write_indent(out, depth, opts.indent);
        out += ']';
    }

    // --- Maps (string keys only) ---
    template <json_map T>
    void serialize(std::string& out, T const& map, json_opts const& opts, int depth) {
        out += '{';
        bool first = true;
        for (auto const& [key, val] : map) {
            if (!first) out += ',';
            first = false;
            if (opts.indent >= 0) write_indent(out, depth + 1, opts.indent);
            out += escape(std::string_view(key));
            out += ':';
            if (opts.indent >= 0) out += ' ';
            serialize(out, val, opts, depth + 1);
        }
        if (!first && opts.indent >= 0)
            write_indent(out, depth, opts.indent);
        out += '}';
    }

    // --- Reflectable Structs ---
    template <json_struct T>
    void serialize(std::string& out, T const& obj, json_opts const& opts, int depth) {
        out += '{';
        bool first = true;

        template for (constexpr auto member : json_members_of(^^T)) {
            // Optionally skip nullopt fields
            if constexpr (reflect::is_optional<typename[:std::meta::type_of(member):]>) {
                if (opts.skip_nullopt && !obj.[:member:].has_value())
                    continue;
            }

            if (!first) out += ',';
            first = false;

            if (opts.indent >= 0) write_indent(out, depth + 1, opts.indent);

            out += escape(std::meta::identifier_of(member));
            out += ':';
            if (opts.indent >= 0) out += ' ';

            serialize(out, obj.[:member:], opts, depth + 1);
        }

        if (!first && opts.indent >= 0)
            write_indent(out, depth, opts.indent);
        out += '}';
    }

} // namespace json_detail

// ---------------------------------------------------------------------------
// to_json(value) — serialize any supported value to a JSON string
// ---------------------------------------------------------------------------

template <typename T>
std::string to_json(T const& value, json_opts const& opts = {}) {
    std::string out;
    out.reserve(256);
    json_detail::serialize(out, value, opts, 0);
    return out;
}

// =========================================================================
// DESERIALIZATION (JSON string → struct)
// =========================================================================

namespace json_detail {

    // Recursive-descent JSON parser.
    //
    // Tokenization is strict: skip_ws() runs ONCE at the start of each
    // value/key/structural-character read, NOT between every byte. So
    // `t r u e` does not parse — only `true` does. (This was bug C3 in
    // the prior implementation.)
    struct parser {
        std::string_view src;
        std::size_t pos = 0;

        // Recursion guard. Threaded through every nested call so that a
        // pathological payload (`[[[…]]]`) cannot exhaust the OS stack.
        int depth = 0;
        static constexpr int max_depth = 1024;

        struct depth_guard {
            parser& p;
            explicit depth_guard(parser& pp) : p(pp) {
                if (p.depth >= p.max_depth)
                    throw json_parse_error("max nesting depth exceeded");
                ++p.depth;
            }
            ~depth_guard() { --p.depth; }
            depth_guard(depth_guard const&) = delete;
            depth_guard& operator=(depth_guard const&) = delete;
        };

        // ---- low-level tokenization ----

        void skip_ws() {
            while (pos < src.size() && (src[pos] == ' ' || src[pos] == '\n' ||
                                        src[pos] == '\r' || src[pos] == '\t'))
                ++pos;
        }

        char peek() {
            skip_ws();
            return pos < src.size() ? src[pos] : '\0';
        }

        // Consume one expected structural character (with leading whitespace).
        // After this returns, the cursor is positioned strictly after `c`.
        void expect(char c) {
            skip_ws();
            if (pos >= src.size() || src[pos] != c) {
                std::string msg = "expected '";
                msg += c;
                msg += "'";
                if (pos < src.size()) {
                    msg += ", got '";
                    msg += src[pos];
                    msg += "'";
                } else {
                    msg += " at end of input";
                }
                throw json_parse_error(msg);
            }
            ++pos;
        }

        // Match a fixed keyword (e.g. "true") with NO whitespace inside it.
        // Skips ws before the keyword, then consumes exactly the literal bytes.
        void expect_keyword(std::string_view kw) {
            skip_ws();
            if (src.size() - pos < kw.size() ||
                src.compare(pos, kw.size(), kw) != 0) {
                throw json_parse_error("expected keyword '" + std::string(kw) + "'");
            }
            pos += kw.size();
        }

        // ---- string parsing ----

        unsigned parse_hex4() {
            if (pos + 4 > src.size())
                throw json_parse_error("bad \\u escape");
            unsigned val = 0;
            for (int i = 0; i < 4; ++i) {
                char h = src[pos + i];
                val <<= 4;
                if (h >= '0' && h <= '9') val |= (h - '0');
                else if (h >= 'a' && h <= 'f') val |= (h - 'a' + 10);
                else if (h >= 'A' && h <= 'F') val |= (h - 'A' + 10);
                else throw json_parse_error("bad hex in \\u");
            }
            pos += 4;
            return val;
        }

        static void encode_utf8(std::string& out, unsigned cp) {
            if (cp < 0x80) {
                out += static_cast<char>(cp);
            } else if (cp < 0x800) {
                out += static_cast<char>(0xC0 | (cp >> 6));
                out += static_cast<char>(0x80 | (cp & 0x3F));
            } else if (cp < 0x10000) {
                out += static_cast<char>(0xE0 | (cp >> 12));
                out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                out += static_cast<char>(0x80 | (cp & 0x3F));
            } else {
                out += static_cast<char>(0xF0 | (cp >> 18));
                out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
                out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                out += static_cast<char>(0x80 | (cp & 0x3F));
            }
        }

        std::string parse_string() {
            expect('"');
            std::string result;
            while (pos < src.size() && src[pos] != '"') {
                if (src[pos] == '\\') {
                    ++pos;
                    if (pos >= src.size()) throw json_parse_error("unterminated string");
                    switch (src[pos]) {
                        case '"':  result += '"';  break;
                        case '\\': result += '\\'; break;
                        case '/':  result += '/';  break;
                        case 'b':  result += '\b'; break;
                        case 'f':  result += '\f'; break;
                        case 'n':  result += '\n'; break;
                        case 'r':  result += '\r'; break;
                        case 't':  result += '\t'; break;
                        case 'u': {
                            ++pos;
                            unsigned cp = parse_hex4();
                            if (cp >= 0xD800 && cp <= 0xDBFF) {
                                // High surrogate — must be followed by \uDC00-\uDFFF
                                if (pos + 1 < src.size() && src[pos] == '\\' && src[pos + 1] == 'u') {
                                    pos += 2;
                                    unsigned lo = parse_hex4();
                                    if (lo < 0xDC00 || lo > 0xDFFF)
                                        throw json_parse_error("invalid low surrogate in \\u escape");
                                    cp = 0x10000 + (cp - 0xD800) * 0x400 + (lo - 0xDC00);
                                } else {
                                    throw json_parse_error("lone high surrogate in \\u escape");
                                }
                            } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                                throw json_parse_error("lone low surrogate in \\u escape");
                            }
                            encode_utf8(result, cp);
                            continue; // pos was advanced inside the case; skip the trailing ++pos
                        }
                        default:
                            throw json_parse_error("bad escape character");
                    }
                    ++pos;
                } else {
                    result += src[pos];
                    ++pos;
                }
            }
            if (pos >= src.size()) throw json_parse_error("unterminated string");
            ++pos; // consume closing quote
            return result;
        }

        // ---- number parsing ----
        //
        // We parse the number's lexeme exactly as written, then dispatch
        // by target type. Integers go through std::from_chars<T>, so
        // `9007199254740993` round-trips losslessly into int64_t — fixing
        // bug C2 (precision loss via double).

        std::string_view scan_number() {
            skip_ws();
            auto start = pos;
            if (pos < src.size() && src[pos] == '-') ++pos;
            while (pos < src.size() && src[pos] >= '0' && src[pos] <= '9') ++pos;
            if (pos < src.size() && src[pos] == '.') {
                ++pos;
                while (pos < src.size() && src[pos] >= '0' && src[pos] <= '9') ++pos;
            }
            if (pos < src.size() && (src[pos] == 'e' || src[pos] == 'E')) {
                ++pos;
                if (pos < src.size() && (src[pos] == '+' || src[pos] == '-')) ++pos;
                while (pos < src.size() && src[pos] >= '0' && src[pos] <= '9') ++pos;
            }
            auto sv = src.substr(start, pos - start);
            if (sv.empty() || sv == "-")
                throw json_parse_error("expected number");
            return sv;
        }

        template <typename T>
            requires std::is_integral_v<T>
        T parse_integer() {
            auto sv = scan_number();
            T val{};
            auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), val);
            if (ec != std::errc{} || ptr != sv.data() + sv.size())
                throw json_parse_error("invalid integer: " + std::string(sv));
            return val;
        }

        template <typename T>
            requires std::is_floating_point_v<T>
        T parse_floating() {
            auto sv = scan_number();
            // libc++ ships float from_chars late; use strtod for portability.
            char buf[64];
            if (sv.size() >= sizeof(buf))
                throw json_parse_error("number too long");
            std::memcpy(buf, sv.data(), sv.size());
            buf[sv.size()] = '\0';
            errno = 0;
            char* end = nullptr;
            double val = std::strtod(buf, &end);
            if (errno == ERANGE || end != buf + sv.size())
                throw json_parse_error("invalid number: " + std::string(sv));
            return static_cast<T>(val);
        }

        // ---- skip_value (used to ignore unknown keys) ----

        void skip_value() {
            depth_guard g{*this};
            char c = peek();
            if (c == '"') { parse_string(); return; }
            if (c == '{') {
                expect('{');
                if (peek() != '}') {
                    do {
                        parse_string();
                        expect(':');
                        skip_value();
                    } while (peek() == ',' && (++pos, true));
                }
                expect('}');
                return;
            }
            if (c == '[') {
                expect('[');
                if (peek() != ']') {
                    do { skip_value(); } while (peek() == ',' && (++pos, true));
                }
                expect(']');
                return;
            }
            if (c == 't') { expect_keyword("true");  return; }
            if (c == 'f') { expect_keyword("false"); return; }
            if (c == 'n') { expect_keyword("null");  return; }
            scan_number();
        }

        // ---- typed value parsing ----
        //
        // Single template, dispatched by `if constexpr`. This keeps every
        // case visible in one place and avoids the out-of-line constrained
        // primary template trap that broke under newer Clang.

        template <typename T>
        T parse_value() {
            using U = std::remove_cvref_t<T>;

            if constexpr (std::is_same_v<U, bool>) {
                char c = peek();
                if (c == 't') { expect_keyword("true");  return true;  }
                if (c == 'f') { expect_keyword("false"); return false; }
                throw json_parse_error("expected boolean");
            }
            else if constexpr (std::is_integral_v<U>) {
                return parse_integer<U>();
            }
            else if constexpr (std::is_floating_point_v<U>) {
                return parse_floating<U>();
            }
            else if constexpr (reflect::is_string_like<U>) {
                return U(parse_string());
            }
            else if constexpr (reflect::is_optional<U>) {
                if (peek() == 'n') {
                    expect_keyword("null");
                    return U{};
                }
                using V = typename U::value_type;
                return U{parse_value<V>()};
            }
            else if constexpr (std::is_enum_v<U>) {
                if (peek() == '"') {
#if REFLECT_JSON_HAS_ENUM
                    auto name = parse_string();
                    auto val = reflect::enum_from_string<U>(name);
                    if (!val) throw json_parse_error("unknown enum value: " + name);
                    return *val;
#else
                    throw json_parse_error("enum string parsing requires reflect/enum.hpp");
#endif
                }
                using I = std::underlying_type_t<U>;
                return static_cast<U>(parse_integer<I>());
            }
            else if constexpr (json_map<U>) {
                depth_guard g{*this};
                U result;
                expect('{');
                if (peek() != '}') {
                    do {
                        auto key = parse_string();
                        expect(':');
                        auto val = parse_value<typename U::mapped_type>();
                        result.emplace(std::move(key), std::move(val));
                    } while (peek() == ',' && (++pos, true));
                }
                expect('}');
                return result;
            }
            else if constexpr (json_array<U> && requires(U t, typename U::value_type v) {
                t.push_back(std::move(v));
            }) {
                depth_guard g{*this};
                U result;
                expect('[');
                if (peek() != ']') {
                    do {
                        result.push_back(parse_value<typename U::value_type>());
                    } while (peek() == ',' && (++pos, true));
                }
                expect(']');
                return result;
            }
            else if constexpr (json_struct<U>) {
                depth_guard g{*this};
                U obj{};
                expect('{');
                if (peek() != '}') {
                    do {
                        auto key = parse_string();
                        expect(':');

                        bool matched = false;
                        template for (constexpr auto member : json_members_of(^^U)) {
                            if (!matched && key == std::meta::identifier_of(member)) {
                                using MemberType = typename[:std::meta::type_of(member):];
                                obj.[:member:] = parse_value<MemberType>();
                                matched = true;
                            }
                        }
                        if (!matched) {
                            skip_value(); // ignore unknown fields
                        }
                    } while (peek() == ',' && (++pos, true));
                }
                expect('}');
                return obj;
            }
            else {
                static_assert(sizeof(T) == 0,
                    "from_json: unsupported type — must be arithmetic, "
                    "string, optional, enum, map, range, or a reflectable struct.");
            }
        }
    };

} // namespace json_detail

// ---------------------------------------------------------------------------
// from_json<T>(str) — deserialize a JSON string into T
// ---------------------------------------------------------------------------

template <typename T>
T from_json(std::string_view json_str) {
    json_detail::parser p{json_str};
    auto result = p.parse_value<T>();
    return result;
}

} // namespace reflect

#endif // REFLECT_JSON_HPP
