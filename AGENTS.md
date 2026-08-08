# Crumb Guide

## Project Overview

Crumb is a C++ command line tool that captures filesystem metadata in local manifests and supports indexed metadata search.

## Development Commands

Use `bin/build` to configure and build. Use `bin/dev` to build, test, and run the executable. Use `ctest` from the `build` directory to run tests.

## Deploy

This project has no deployment process. Build artifacts are local command line executables.

## Architecture Overview

`lib/domain` owns business rules and value objects. `lib/ports` owns application contracts. `application` owns use case orchestration. `infrastructure` implements ports and owns the composition root. `boundary` translates CLI input and output.

Key insight: the domain does not depend on delivery or persistence details. Boundaries depend on use cases through explicit contracts.

## Core Domain Models

`DirectoryManifest` owns the files known for one directory. `FileEntry` links a stable file identity, name, metadata, and fingerprint. `SearchIndex` is a domain projection built from manifest entries. Value objects validate identifiers, paths, names, fingerprints, and content hashes.

## Tools

Use the Vault MCP to retrieve team standards. Use `bin/build` and `ctest` for local verification.
