#pragma once

#include "application/rebuild_search_index.hpp"
#include "application/reconcile_directory/reconcile.hpp"
#include "application/search_manifest/search.hpp"
#include "plugins/google_drive/google_drive_plugin/drive_file_system.hpp"
#include "plugins/google_drive/google_drive_plugin/drive_manifest_repository.hpp"
#include "plugins/google_drive/google_drive_plugin/drive_metadata_extractor.hpp"
#include "plugins/google_drive/google_drive_plugin/drive_search_index_repository.hpp"

#include <cstddef>
#include <expected>
#include <optional>
#include <string>
#include <string_view>

namespace crumb::plugins::google_drive {

struct DriveIndexResult {
    domain::DirectoryPath directory;
    application::ReconcileResult reconcile;
};

class GoogleDrivePlugin final {
   public:
    GoogleDrivePlugin(infrastructure::NativeFileSystem& native,
                      ports::ManifestRepository& manifests, ports::SearchIndexRepository& index,
                      ports::FingerprintService& fingerprints, ports::IdGenerator& ids,
                      ports::Clock& clock)
        : filesystem_(native),
          extractor_(),
          manifests_(manifests),
          index_(index),
          reconcile_(manifests_, filesystem_, fingerprints, extractor_, ids, clock),
          rebuild_index_(manifests_, filesystem_, index_),
          search_(manifests_, filesystem_, &index_) {}

    [[nodiscard]] std::expected<domain::DirectoryPath, std::string> resolve(
        std::optional<std::string_view> requested_path = std::nullopt) const;
    [[nodiscard]] std::expected<DriveIndexResult, std::string> index(
        std::optional<std::string_view> requested_path = std::nullopt);
    [[nodiscard]] std::expected<application::SearchResult, std::string> search(
        const domain::DirectoryPath& directory, std::string_view query, std::size_t limit);

    [[nodiscard]] static std::string url_for_item_id(std::string_view item_id);

   private:
    DriveFileSystem filesystem_;
    DriveMetadataExtractor extractor_;
    DriveManifestRepository manifests_;
    DriveSearchIndexRepository index_;
    application::ReconcileDirectory reconcile_;
    application::RebuildSearchIndex rebuild_index_;
    application::SearchManifest search_;
};

}  // namespace crumb::plugins::google_drive
