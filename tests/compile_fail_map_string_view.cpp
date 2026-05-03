// Compile-failure test: from_json refuses to deserialize a map keyed
// by std::string_view (or any non-owning string type). This file MUST
// fail to compile — the corresponding CMake target is wired up with
// WILL_FAIL TRUE so a successful build of this file means the
// concept rejection is broken.
//
// See include/reflect/json.hpp — the json_map concept is constrained
// to std::string keys to prevent dangling views in the deserialized
// map.

#include <reflect/json.hpp>

#include <map>
#include <string_view>

int main() {
    auto m = reflect::from_json<std::map<std::string_view, int>>("{}");
    (void)m;
    return 0;
}
