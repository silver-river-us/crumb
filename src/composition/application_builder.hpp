#pragma once
#include "search/application/index_size.hpp"
#include "manifests/application/reconcile_directory/reconcile.hpp"
#include "search/application/rebuild_search_index.hpp"
#include "search/application/search_manifest/search.hpp"
#include "files/infrastructure/native_filesystem.hpp"
#include "files/infrastructure/streaming_hash.hpp"
#include "manifests/infrastructure/toml_manifest_repository.hpp"
#include "search/infrastructure/binary_search_index_repository.hpp"
#include "files/infrastructure/system_clock.hpp"
#include "files/infrastructure/ulid_generator.hpp"
#include "google_drive/infrastructure/google_drive_plugin/plugin.hpp"
namespace crumb::infrastructure {
struct ApplicationComponents {
    TomlManifestRepository manifests;
    BinarySearchIndexRepository index;
    NativeFileSystem filesystem;
    StreamingHash hashes{""};
    UlidGenerator ids;
    SystemClock clock;
    plugins::google_drive::GoogleDrivePlugin drive;
    application::IndexSize index_size;
    application::ReconcileDirectory reconcile;
    application::RebuildSearchIndex rebuild_index;
    application::SearchManifest search;
    ApplicationComponents()
        : drive(filesystem, manifests, index, hashes, ids, clock),
          index_size(index),
          reconcile(manifests, filesystem, hashes, filesystem, ids, clock),
          rebuild_index(manifests, filesystem, index),
          search(manifests, filesystem, &index) {}
};
}  // namespace crumb::infrastructure
