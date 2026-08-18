# Crumb Guide

## Project Overview

Crumb is a C++ command line tool that captures filesystem metadata in local manifests and supports indexed metadata search.

## Development Commands

Use `bin/build` to configure and build. Use `bin/dev` to build, test, and run the executable. Use `ctest` from the `build` directory to run tests.

## Deploy

This project has no deployment process. Build artifacts are local command line executables.

## Architecture Overview

Crumb uses screaming architecture: top-level source modules name the product
capabilities rather than technical layers. `files`, `manifests`, and `search`
each own their domain models, application contracts/use cases, and
infrastructure adapters. `google_drive` owns its integration adapter,
`presentation` owns the CLI boundary, and `composition` wires the capabilities
together.

Key insight: capabilities own their technical layers. Dependencies cross
capabilities through explicit contracts, while the CLI remains a presentation
detail rather than the organizing principle of the source tree.

## Core Domain Models

`DirectoryManifest` owns the files known for one directory. `FileEntry` links a stable file identity, name, metadata, and fingerprint. `SearchIndex` is a domain projection built from manifest entries. The `files` capability owns value objects for identifiers, paths, names, fingerprints, and content hashes.

## Tools

Use the Vault MCP to retrieve team standards. Use `bin/build` and `ctest` for local verification.
