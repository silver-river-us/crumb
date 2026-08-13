#pragma once

#include "application/rebuild_search_index.hpp"
#include "application/reconcile_directory.hpp"
#include "application/search_manifest.hpp"
#include "infrastructure/filesystem/native_filesystem.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <sys/types.h>

namespace crumb::plugins::google_drive {

namespace testing {
using PipeFunction = int (*)(int*);
using ForkFunction = pid_t (*)();
extern PipeFunction pipe_function;
extern ForkFunction fork_process;
std::optional<std::string> command_output_for_test(const std::string& command, std::size_t limit,
                                                   std::chrono::milliseconds timeout);
std::optional<std::string> extract_office_text_for_test(const std::filesystem::path& path);
}  // namespace testing

class DriveManifestRepository final : public ports::ManifestRepository {
   public:
    explicit DriveManifestRepository(ports::ManifestRepository& delegate) : delegate_(delegate) {}

    void set_source_root(const domain::DirectoryPath& source_root);
    std::expected<std::optional<domain::DirectoryManifest>, std::string> load(
        const domain::DirectoryPath&) override;
    std::expected<void, std::string> save(const domain::DirectoryManifest&) override;

   private:
    [[nodiscard]] std::filesystem::path cache_path(const domain::DirectoryPath&) const;
    [[nodiscard]] static domain::DirectoryManifest with_path(const domain::DirectoryManifest&,
                                                             domain::DirectoryPath);

    ports::ManifestRepository& delegate_;
    std::filesystem::path source_root_;
    std::filesystem::path cache_root_;
};

class DriveSearchIndexRepository final : public ports::SearchIndexRepository {
   public:
    explicit DriveSearchIndexRepository(ports::SearchIndexRepository& delegate)
        : delegate_(delegate) {}

    void set_cache_root(std::filesystem::path cache_root) { cache_root_ = std::move(cache_root); }
    std::expected<void, std::string> save(const domain::DirectoryPath&,
                                          const domain::SearchIndex&) override;
    std::expected<domain::SearchIndex, std::string> load(
        const domain::DirectoryPath&) const override;
    std::expected<std::uintmax_t, std::string> size(const domain::DirectoryPath&) const override;

   private:
    ports::SearchIndexRepository& delegate_;
    std::filesystem::path cache_root_;
};

class DriveFileSystem final : public ports::FileSystem {
   public:
    explicit DriveFileSystem(infrastructure::NativeFileSystem& native) : native_(native) {}

    std::expected<std::vector<domain::FileSnapshot>, std::string> list_regular_files(
        const domain::DirectoryPath&) override;
    std::expected<std::optional<std::string>, std::string> read_text_file(
        const domain::DirectoryPath&, const domain::FileName&) override;
    std::expected<std::vector<std::pair<domain::DirectoryPath, domain::FileName>>, std::string>
    list_regular_files_recursive(const domain::DirectoryPath&) override;
    std::expected<std::vector<domain::DirectoryPath>, std::string> list_directories_recursive(
        const domain::DirectoryPath&) override;

   private:
    infrastructure::NativeFileSystem& native_;
};

class DriveMetadataExtractor final : public ports::MetadataExtractor {
   public:
    DriveMetadataExtractor() = default;

    std::expected<domain::FileMetadata, std::string> extract(const domain::DirectoryPath&,
                                                             const domain::FileName&,
                                                             domain::FileMetadata) override;
};

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
