# crumb

Crumb stores filesystem metadata in `.crumb` manifests and creates a compact search index. It records metadata only. It never stores original file content or previews.

## Architecture

`crumb_lib` owns the domain model and port contracts. `crumb_application` owns use cases. `crumb_boundary` translates CLI input and output. `crumb_infrastructure` owns filesystem, storage, hashing, clock, and identity adapters. The composition root joins these bounded contexts.

## Build and test

```sh
bin/build
cd build
ctest
```

## Scan

```sh
build/crumb scan path/to/directory
```

The scan reports `scanned`, `added`, `updated`, `removed`, and elapsed time. It refreshes manifests before rebuilding the search index.

## Search

```sh
build/crumb search "technical proposal" limit 10
build/crumb search path/to/directory "technical proposal" limit 10
```

Search uses case insensitive fuzzy matching and rank weighting over file names, metadata, tags, extension fields, and persisted index terms.

Use `--tap` to show a query waterfall in the terminal:

```sh
build/crumb search "technical proposal" --tap
```

Use `--tap html` (or `--tap=html`) to emit a self contained HTML report, which can be redirected to a file:

```sh
build/crumb search "technical proposal" --tap html > search_tap.html
```

## Index size

```sh
build/crumb index_size path/to/directory
```

The project includes `bin/build`, `bin/dev`, `bin\build.cmd`, and `bin\dev.cmd` helpers.
