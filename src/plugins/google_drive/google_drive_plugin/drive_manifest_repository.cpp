#include "plugins/google_drive/google_drive_plugin/drive_manifest_repository.hpp"

#include "plugins/google_drive/google_drive_plugin/support/details.hpp"

#include <exception>
#include <utility>

namespace crumb::plugins::google_drive {

void DriveManifestRepository::set_source_root(const domain::DirectoryPath& source_root) {
    source_root_ = source_root.value();
    cache_root_ = detail::cache_root_for(source_root);
}

std::filesystem::path DriveManifestRepository::cache_path(
    const domain::DirectoryPath& source_directory) const {
    const auto relative =
        std::filesystem::path(source_directory.value()).lexically_relative(source_root_);
    return relative.empty() || relative == "." ? cache_root_ : cache_root_ / relative;
}

domain::DirectoryManifest DriveManifestRepository::with_path(
    const domain::DirectoryManifest& manifest, domain::DirectoryPath path) {
    auto copy = domain::DirectoryManifest::create(manifest.id(), std::move(path),
                                                  manifest.generated_at(), manifest.generator());
    for (const auto& entry : manifest.files()) copy.add(entry);
    return copy;
}

std::expected<std::optional<domain::DirectoryManifest>, std::string> DriveManifestRepository::load(
    const domain::DirectoryPath& source_directory) {
    if (source_root_.empty()) return std::unexpected("Drive storage root is not configured");
    auto loaded =
        delegate_.load(domain::DirectoryPath::create(cache_path(source_directory).string()));
    if (!loaded) return std::unexpected(loaded.error());
    if (!loaded.value()) return std::nullopt;
    return std::optional<domain::DirectoryManifest>(with_path(*loaded.value(), source_directory));
}

std::expected<void, std::string> DriveManifestRepository::save(
    const domain::DirectoryManifest& manifest) {
    if (source_root_.empty()) return std::unexpected("Drive storage root is not configured");
    const auto destination = cache_path(manifest.path());
    try {
        std::filesystem::create_directories(destination);
    } catch (const std::exception& error) {
        return std::unexpected(error.what());
    }
    return delegate_.save(with_path(manifest, domain::DirectoryPath::create(destination.string())));
}

}  // namespace crumb::plugins::google_drive
