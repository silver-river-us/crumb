#pragma once
#include "lib/application/reconcile_directory.hpp"
#include "infrastructure/filesystem/native_filesystem.hpp"
#include "infrastructure/hashing/streaming_hash.hpp"
#include "infrastructure/persistence/toml_manifest_repository.hpp"
#include "infrastructure/system/system_clock.hpp"
#include "infrastructure/system/ulid_generator.hpp"
namespace crumb::infrastructure {
struct ApplicationComponents {
    TomlManifestRepository manifests;
    NativeFileSystem filesystem;
    StreamingHash hashes{""};
    UlidGenerator ids;
    SystemClock clock;
    application::ReconcileDirectory reconcile;
    ApplicationComponents() : reconcile(manifests, filesystem, hashes, filesystem, ids, clock) {}
};
}
