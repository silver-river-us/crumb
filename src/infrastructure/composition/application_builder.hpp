#pragma once
#include "lib/application/reconcile_directory.hpp"
#include "lib/application/search_manifest.hpp"
#include "infrastructure/filesystem/native_filesystem.hpp"
#include "infrastructure/hashing/streaming_hash.hpp"
#include "infrastructure/persistence/toml_manifest_repository.hpp"
#include "infrastructure/persistence/binary_search_index_repository.hpp"
#include "infrastructure/system/system_clock.hpp"
#include "infrastructure/system/ulid_generator.hpp"
namespace crumb::infrastructure {
struct ApplicationComponents {
    TomlManifestRepository manifests;
    BinarySearchIndexRepository index;
    NativeFileSystem filesystem;
    StreamingHash hashes{""};
    UlidGenerator ids;
    SystemClock clock;
    application::ReconcileDirectory reconcile;
    application::SearchManifest search;
    ApplicationComponents()
        : reconcile(manifests, filesystem, hashes, filesystem, ids, clock, &index), search(manifests, filesystem, &index) {}
};
}
