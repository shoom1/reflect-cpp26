# reflect

**C++26 reflection utilities. Header-only. Zero dependencies. Zero boilerplate.**

Turn this:

```cpp
// nlohmann/json — 12 lines of boilerplate PER STRUCT
struct Trade {
    int id; std::string symbol; double price;
};
NLOHMANN_DEFINE_TYPE_INTRUSIVE(Trade, id, symbol, price) // manual field listing
```

Into this:

```cpp
// reflect — zero boilerplate, ever
struct Trade {
    int id; std::string symbol; double price;
};
auto json = reflect::to_json(trade);        // just works
auto t    = reflect::from_json<Trade>(json); // just works
```

---

## What's inside

| Header | What it does | Inspired by |
|--------|-------------|-------------|
| `reflect/enum.hpp` | Enum ↔ string, names, values, flags | magic_enum |
| `reflect/print.hpp` | Pretty-print any struct | Rust `derive(Debug)` |
| `reflect/compare.hpp` | Automatic ==, <=>, std::hash | Rust `derive(PartialEq, Hash)` |
| `reflect/tuple.hpp` | Struct ↔ tuple conversion | Boost.PFR |
| `reflect/json.hpp` | JSON serialize/deserialize | nlohmann/json, glaze |
| `reflect/visit.hpp` | `for_each_field(obj, visitor)` | Boost.PFR |
| `reflect/args.hpp` | CLI parsing from a struct | Rust's clap |
| `reflect/diff.hpp` | Struct diff / change detection | — |
| `reflect/format.hpp` | `std::format` integration | — |
| `reflect/type_name.hpp` | Clean demangled type names | Boost.TypeIndex |

Every header is independent. Include only what you need.

---

## Quick start

```cpp
#include <reflect/enum.hpp>
#include <reflect/print.hpp>
#include <reflect/json.hpp>

enum class Color { red, green, blue };

struct Point { int x; int y; };
struct Line  { Point start; Point end; std::string label; };

int main() {
    // Enum reflection
    reflect::enum_to_string(Color::red);           // → "red"
    reflect::enum_from_string<Color>("blue");       // → Color::blue
    reflect::enum_count<Color>();                   // → 3

    // Pretty-print
    Line line{{0,0}, {10,20}, "diagonal"};
    reflect::to_string(line);
    // → Line{.start=Point{.x=0, .y=0}, .end=Point{.x=10, .y=20}, .label="diagonal"}

    // JSON (nested structs, vectors, optionals — all automatic)
    reflect::to_json(line);
    // → {"start":{"x":0,"y":0},"end":{"x":10,"y":20},"label":"diagonal"}

    auto line2 = reflect::from_json<Line>(R"({"start":{"x":1,"y":2},"end":{"x":3,"y":4},"label":"test"})");
}
```

Compile:

```bash
# Bloomberg Clang fork (recommended for now)
clang++ -std=c++26 -freflection-latest -Iinclude main.cpp

# GCC trunk / GCC 16+
g++ -std=c++26 -Iinclude main.cpp
```

---

## Try it locally (Docker)

Don't have a P2996-capable compiler installed? The repo ships a pinned
toolchain. Requires only Docker:

```bash
git clone https://github.com/shoom1/reflect-cpp26.git
cd reflect-cpp26
./scripts/docker-build.sh        # first run: builds toolchain image (~30–60 min) + project
./scripts/docker-test.sh         # runs all tests
./scripts/docker-run.sh reflect_example_showcase
```

The `Dockerfile` builds Bloomberg's clang-p2996 fork from source pinned
to a specific commit. Trust chain: Docker Desktop → ubuntu:24.04 →
bloomberg/clang-p2996. No third-party prebuilt images.

The first build takes 30–60 minutes (LLVM compile). Subsequent runs are
seconds — Docker caches the image, and `cmake --build` is incremental.

CI mirrors this setup — see `.github/workflows/`.

---

## Feature details

### Enum reflection

```cpp
#include <reflect/enum.hpp>

enum class Status { pending, active, closed };

reflect::enum_to_string(Status::active);       // → "active"
reflect::enum_from_string<Status>("closed");    // → std::optional<Status>{Status::closed}
reflect::enum_from_string<Status>("invalid");   // → std::nullopt
reflect::enum_count<Status>();                  // → 3
reflect::enum_names<Status>();                  // → std::array{"pending", "active", "closed"}
reflect::enum_values<Status>();                 // → std::array{Status::pending, ...}
reflect::enum_entries<Status>();                // → std::array{pair{Status::pending, "pending"}, ...}
reflect::enum_contains<Status>(1);             // → true
reflect::enum_cast<Status>(99);                // → std::nullopt
reflect::enum_index(Status::active);           // → 1

// Bitmask / flags
enum class Perm : unsigned { read = 1, write = 2, exec = 4 };
template <> struct reflect::enable_bitmask_operators<Perm> : std::true_type {};

auto p = Perm::read | Perm::exec;             // works thanks to enable_bitmask_operators
reflect::flags_to_string(p);                   // → "read|exec"
reflect::flags_from_string<Perm>("read|write"); // → Perm::read | Perm::write
```

### Struct field iteration

```cpp
#include <reflect/visit.hpp>

struct Config { std::string host; int port; bool tls; };
Config cfg{"localhost", 8080, true};

// Iterate fields
reflect::for_each_field(cfg, [](std::string_view name, auto const& value) {
    std::cout << name << ": " << value << "\n";
});

// Field metadata
reflect::field_count<Config>();   // → 3
reflect::field_names<Config>();   // → {"host", "port", "tls"}
reflect::has_field<Config>("port"); // → true

// Mutate fields
reflect::for_each_field(cfg, [](std::string_view name, auto& value) {
    if constexpr (std::is_same_v<std::remove_cvref_t<decltype(value)>, int>)
        value *= 2;
});
// cfg.port is now 16160
```

### JSON

```cpp
#include <reflect/json.hpp>

struct Order {
    int id;
    std::string symbol;
    double price;
    std::optional<std::string> note;
    std::vector<int> fills;
};

Order order{1, "MSFT", 420.69, "limit", {100, 200, 50}};

// Serialize
reflect::to_json(order);
// → {"id":1,"symbol":"MSFT","price":420.69,"note":"limit","fills":[100,200,50]}

// Pretty-print
reflect::to_json(order, {.indent = 2});

// Skip nullopt fields
reflect::to_json(order, {.skip_nullopt = true});

// Deserialize
auto order2 = reflect::from_json<Order>(json_string);

// Unknown fields in JSON are silently ignored — forward-compatible
```

### Tuple conversion

```cpp
#include <reflect/tuple.hpp>

struct Vec3 { float x, y, z; };
Vec3 v{1.0f, 2.0f, 3.0f};

auto t = reflect::to_tuple(v);            // std::tuple<float,float,float>
auto v2 = reflect::from_tuple<Vec3>(t);   // Vec3{1, 2, 3}

reflect::get<0>(v);                        // → 1.0f
reflect::apply(v, [](float x, float y, float z) { return x + y + z; }); // → 6.0f
```

### CLI argument parsing

```cpp
#include <reflect/args.hpp>

struct Args {
    std::string input;                   // --input <STRING> (required)
    int verbose = 0;                     // --verbose <INT> (required)
    bool debug = false;                  // --debug (flag)
    std::optional<std::string> output;   // --output <STRING> (optional)
    std::vector<std::string> files;      // --files a.txt b.txt (multi-value)
};

int main(int argc, char** argv) {
    auto args = reflect::parse_args<Args>(argc, argv);
    // Automatically handles:
    //   --help / -h → prints generated help text
    //   --input foo.txt --verbose 3 --debug --files a.txt b.txt
    //   Missing required values → throws reflect::args_error
    //   Unknown flags → throws reflect::args_error
    //   Field name underscores → dashes: my_field → --my-field
}

// Generate help text:
reflect::args_help<Args>("myapp");
// Usage: myapp [OPTIONS]
//
// Options:
//   --input <STRING>        (required)
//   --verbose <INT>         (required)
//   --debug                  (flag)
//   --output <STRING>        (optional)
//   --files <STRING>...      (multi-value)
//   --help                   Show this help message
```

### Struct diff

```cpp
#include <reflect/diff.hpp>

struct Config { std::string host; int port; bool tls; };
Config a{"localhost", 8080, false};
Config b{"localhost", 9090, true};

reflect::has_changes(a, b);            // → true
reflect::change_count(a, b);           // → 2

auto names = reflect::changed_field_names(a, b);
// → {"port", "tls"}

reflect::diff_summary(a, b);
// → "port: 8080 → 9090, tls: 0 → 1"

auto entries = reflect::diff(a, b);
// entries[0] = {.name="host", .index=0, .changed=false}
// entries[1] = {.name="port", .index=1, .changed=true}
// entries[2] = {.name="tls",  .index=2, .changed=true}

// Selectively apply changes
reflect::apply_changes(a, b);          // copy all changed fields
reflect::apply_changes(a, b, [](auto name, auto& old_v, auto& new_v) {
    return name == "port";             // only copy port
});
```

### std::format integration

```cpp
#include <reflect/format.hpp>

struct Point { int x; int y; };
REFLECT_MAKE_FORMATTABLE(Point);

// Now works with std::format and std::print:
std::format("{}", Point{3, 7});     // → "Point{.x=3, .y=7}"
std::format("{:j}", Point{3, 7});   // → {"x":3,"y":7}
std::format("{:J}", Point{3, 7});   // → pretty JSON

// Without the macro, use reflect::format() directly:
reflect::format(Point{3, 7});       // → "Point{.x=3, .y=7}"
```

---

## Requirements

| Compiler | Version | Flag | Status |
|----------|---------|------|--------|
| Bloomberg Clang fork | `p2996` branch | `-freflection-latest` | ✅ Tested |
| GCC | 16+ (trunk) | `-std=c++26` | Untested |
| EDG (Compiler Explorer) | — | — | Untested |
| Clang mainline | — | — | ❌ Not yet |
| MSVC | — | — | ❌ Not yet |

This library requires C++26 with P2996 reflection support. Try it without
installing anything: open [Compiler Explorer](https://godbolt.org/), pick
**"x86-64 clang (experimental P2996)"** from the compiler dropdown, and
paste in any of the snippets above. Add `-std=c++26 -freflection-latest`
to the compiler flags box.

## Install

**Header-only** — just copy `include/reflect/` into your project:

```bash
cp -r include/reflect/ /your/project/include/
```

Or with CMake:

```cmake
add_subdirectory(reflect)
target_link_libraries(your_target PRIVATE reflect)
```

Or with CMake FetchContent:

```cmake
include(FetchContent)
FetchContent_Declare(reflect
    GIT_REPOSITORY https://github.com/shoom1/reflect-cpp26.git
    GIT_TAG main
)
FetchContent_MakeAvailable(reflect)
target_link_libraries(your_target PRIVATE reflect)
```

## Roadmap

- [x] Enum ↔ string
- [x] Pretty-print
- [x] Comparison & hashing
- [x] Tuple conversion
- [x] JSON serialization
- [x] Field iteration / visitor
- [x] Type names
- [x] CLI argument parsing (struct → arg parser)
- [x] Struct diff (`reflect::diff(a, b)`)
- [x] `std::format` integration
- [ ] Binary serialization (MessagePack / CBOR)
- [ ] P3394 annotation support for field renaming, skip, validation
- [ ] CSV serialization
- [ ] Automatic `std::formatter` without macro (once compilers support it)
- [ ] Config file loading (TOML/YAML → struct)

## License

MIT — do whatever you want.

## Contributing

PRs welcome. Please include tests and keep each header independent.
