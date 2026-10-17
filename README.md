# multi_index_lookup

[![Linux CI](https://github.com/goloveshin/multi_index_lookup/actions/workflows/linux-ci.yml/badge.svg)](https://github.com/goloveshin/multi_index_lookup/actions/workflows/linux-ci.yml) [![Windows CI](https://github.com/goloveshin/multi_index_lookup/actions/workflows/windows-ci.yml/badge.svg)](https://github.com/goloveshin/multi_index_lookup/actions/workflows/windows-ci.yml)

A small header-only wrapper around [Boost.MultiIndex](https://www.boost.org/doc/libs/release/libs/multi_index/doc/index.html).

`multi_index_lookup::container_extended_t` is a Boost.MultiIndex container plus lookup by index tag: find a record, find a range, and modify a record. A missing key is reported either as an empty `std::optional` or by calling a function you pass.

## Requirements

- C++23
- Boost.MultiIndex. If the target `Boost::multi_index` does not exist, the library fetches Boost itself, limited to MultiIndex.

## Use with CMake

```cmake
include(FetchContent)

FetchContent_Declare(
    multi_index_lookup
    GIT_REPOSITORY https://github.com/goloveshin/multi_index_lookup.git
    GIT_TAG main
)
FetchContent_MakeAvailable(multi_index_lookup)

target_link_libraries(my_app PRIVATE multi_index_lookup::multi_index_lookup)
```

```cpp
#include <multi_index_lookup.hpp>

class container_t : public multi_index_lookup::container_extended_t<detail::container_t>
{
};
```

The unit tests and the example are built only when `multi_index_lookup` is the top-level project. To control it from a parent project, set the option `MULTI_INDEX_LOOKUP_BUILD_TESTS`.

## Build the tests

```text
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

CI builds and tests it on Linux (GCC, Clang) and Windows (MSVC), in Debug and Release.

## Layout

| path | what it is |
| --- | --- |
| `include/multi_index_lookup.hpp` | the header |
| `tests/` | unit tests (doctest) |
| `example/` | a container with its own class on top of the library |

## License

[Boost Software License 1.0](LICENSE)
