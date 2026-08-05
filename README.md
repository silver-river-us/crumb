# crumb

Crumb is a filesystem-native metadata manifest tool. Running `crumb scan DIR`
creates or atomically refreshes `DIR/.crumb`, describing only the immediate
regular files in that directory. Original content, full-text extraction,
previews, and nested files are never stored in the manifest.

The implementation follows the format specification's dependency direction:

```text
boundary ──────▶ lib ◀────── infrastructure
```

`crumb_lib` contains domain objects, ports, and application services;
`crumb_boundary` contains CLI translation; and `crumb_infrastructure` contains
the native filesystem, TOML mapper/repository, streamed hashing, clock, and
ULID implementations.

## Build

Configure and build with the CMake presets:

```sh
cmake --preset default
cmake --build --preset default
```

Run the tests with:

```sh
ctest --preset default
```

## Scan a directory

```sh
./build/crumb scan path/to/directory
```

The writer emits UTF-8 TOML with quoted filename keys, required fields before
optional fields, sorted file records, and a final newline. It writes
`.crumb.tmp` in the target directory and renames it to `.crumb` only after the
temporary file has been flushed. Readers reject unsupported major versions and
ignore fields they do not understand.

The repository also provides platform-specific helper scripts:

```sh
# macOS/Linux
bin/build
bin/dev

# Windows
bin\\build.cmd
bin\\dev.cmd
```

Run the executable directly with `build/crumb` on macOS/Linux or
`build\\crumb.exe` on Windows.
