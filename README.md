# crumb

[![CI](https://github.com/silver-river-us/crumb/actions/workflows/quality.yml/badge.svg)](https://github.com/silver-river-us/crumb/actions/workflows/quality.yml)

Crumb stores filesystem metadata in `.crumb` manifests and creates a compact search index. It records metadata only. It never stores original file content or previews.

## Architecture

`crumb_lib` owns the domain model and port contracts. `crumb_application` owns use cases. `crumb_boundary` translates CLI input and output. `crumb_infrastructure` owns filesystem, storage, hashing, clock, and identity adapters. The composition root joins these bounded contexts.

## Build and test

```sh
bin/build
cd build
ctest
```

## Quality checks

Crumb enables `-Wall -Wextra -Wpedantic -Wconversion -Wshadow` for Clang/GCC builds. Set `-DCRUMB_WARNINGS_AS_ERRORS=ON` to promote those warnings to errors.

The repository also provides these checks:

```sh
brew install llvm cppcheck include-what-you-use
bin/lint                         # clang-tidy
cmake --build build --target format-check
cmake --build build --target cppcheck
cmake --build build --target iwyu
bin/check                        # all CI checks locally
bin/sanitize asan-ubsan          # Address + Undefined Behavior sanitizers
bin/sanitize tsan                # Thread sanitizer
bin/coverage                      # LLVM source-based coverage report
```

`bin/check` runs the complete CI quality suite locally: static checks and tests, Clang Static Analyzer memory ownership checks, ASan/UBSan, TSan, and the LLVM coverage report. It requires the CI toolchain (`clang`, `clang-tidy`, `clang-format`, `cppcheck`, Include-What-You-Use, `scan-build`, CMake, and Ninja). The sanitizer commands build and test into dedicated `build/asan-ubsan` and `build/tsan` directories. `bin/coverage` uses the LLVM tools that match the compiler (Xcode’s tools on macOS) and reports coverage for `src/`, excluding tests and system headers.

The CI sanitizer jobs have focused purposes: AddressSanitizer (ASan) detects memory errors, UndefinedBehaviorSanitizer (UBSan) detects invalid C++ operations, and ThreadSanitizer (TSan) detects data races and other threading errors.

The CI also runs Clang Static Analyzer for path-sensitive memory ownership and leak checks. It complements the runtime ASan/LeakSanitizer checks: the static analyzer examines possible paths without running the program, while sanitizers verify behavior during tests.

## Scan

```sh
build/crumb scan path/to/directory
```

The scan reports `scanned`, `added`, `updated`, `removed`, and elapsed time. It refreshes manifests before rebuilding the search index.

## Search

```sh
build/crumb search "technical proposal" --limit 10
build/crumb search path/to/directory "technical proposal" --limit 10
```

Search uses case insensitive fuzzy matching and rank weighting over file names, directory paths, metadata, tags, extension fields, and persisted index terms. Results with equal relevance are ordered newest-first by creation date, then edit date. Every indexed file has a stable `fid:` reference derived from its manifest identity; use that ID in future metadata commands. Multi-word queries first require every query term to match the same result; if that produces no results, Crumb returns ranked partial matches so natural-language queries still surface useful guidance.

Search displays full result cards by default. Use `--table` for a PostgreSQL-style result table with inline clickable links; interactive table output opens in a scrollable pager. `--full` and `--details` remain accepted aliases for the default card view.

### Directory aliases

Crumb optionally loads `~/.crumb.conf` when it starts. Define aliases in TOML under `[aliases]`:

```toml
[aliases]
vault = "~/Develop/vault"
```

An alias can be used wherever a directory is accepted. For example, `build/crumb search vault "technical proposal"` searches `~/Develop/vault`; `build/crumb scan vault` and `build/crumb index_size vault` work the same way. Alias values beginning with `~/` expand to the current user’s home directory.

Use `--tap` to show a query waterfall in the terminal:

```sh
build/crumb search "technical proposal" --tap
```

Use `--tap html` (or `--tap=html`) to emit a self contained HTML report, which can be redirected to a file:

```sh
build/crumb search "technical proposal" --tap html > search_tap.html
```

## Google Drive

When Google Drive for desktop is mounted locally, index it explicitly before searching:

```sh
build/crumb index drive
build/crumb search drive "technical proposal" --limit 10
build/crumb search drive "technical proposal" --limit 10 --table
build/crumb search drive "technical proposal" --tap html > drive_search.html
```

The Drive plugin reads local files and Drive’s local item-id metadata; it does not use the Drive API, OAuth, or a watcher. Search results include the browser URL derived from that metadata, while the result table shows each file’s local creation and edit dates. The default index is safe for streamed Drive mounts: filenames, metadata, links, and readable plain-text files are indexed, while browser-backed Office files use metadata fallback. To attempt bounded local Office extraction for materialized files, set `CRUMB_DRIVE_CONTENT=1` when indexing. Run `crumb index drive` again after local Drive changes. User metadata stored in manifest extension fields is preserved during reindexing; extractor-owned `crumb.*` fields may be refreshed.

## Index size

```sh
build/crumb index_size path/to/directory
```

The project includes `bin/build`, `bin/dev`, `bin\build.cmd`, and `bin\dev.cmd` helpers.
