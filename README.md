# crumb

Crumb is a filesystem-native metadata manifest tool. Running `crumb scan DIR`
recursively creates or atomically refreshes `.crumb` manifests for each
(directory-local) set of regular files. Original content and previews are never
stored in the manifest; searchable terms are stored in a compact, deduplicated,
zlib-compressed inverted index with common connector words removed.

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

The scan prints machine-readable counts and a monotonic elapsed duration with a unit suffix (`elapsed_µs`, `elapsed_ms`, or `elapsed_s`):

```text
directory=path/to/directory scanned=12 added=12 updated=0 removed=0 elapsed_ms=...
```

## Search manifest metadata

Recursively scan a directory to build or refresh its `.crumb` indexes, then query them case-insensitively:

```sh
# From inside the directory being searched
./build/crumb search "technical proposal" --limit 10

# Or provide a directory explicitly
./build/crumb search path/to/directory "technical proposal" --limit 10
```

Search uses case-insensitive fuzzy keyword matching with IDF-style weighting,
then ranks results by score. It searches the persisted filename, content-term,
MIME type, title, author, tag, and extension-field indexes. Results include
matched paths, scores, and timing metrics. Recursive scan keeps the human-readable
`.crumb` manifests and atomically generates a root-level compact zlib-compressed binary `.crumb.index`.
The index stores a global term keymap and document postings with term counts, so
search resolves candidates from postings before reading manifest metadata. Run
`scan` again after changing files. Symlinks, metadata directories, binary files,
and oversized files are excluded from the index.

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
