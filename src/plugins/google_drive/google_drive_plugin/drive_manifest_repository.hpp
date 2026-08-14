#pragma once

#include "domain/directory_manifest.hpp"
#include "ports/manifest_repository.hpp"

#include <expected>
#include <filesystem>
#include <optional>
#include <string>

namespace crumb::plugins::google_drive {

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

}  // namespace crumb::plugins::google_drive
