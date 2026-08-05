# crumb

A minimal C++23 project built with CMake.

## Build

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Run the executable with:

```sh
./build/crumb
```
