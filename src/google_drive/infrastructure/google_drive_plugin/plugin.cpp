#include "google_drive/infrastructure/google_drive_plugin/plugin.hpp"

#include "google_drive/infrastructure/google_drive_plugin/details.hpp"

#include <filesystem>

namespace crumb::plugins::google_drive {

std::expected<domain::DirectoryPath, std::string> GoogleDrivePlugin::resolve(
    std::optional<std::string_view> requested_path) const {
    if (requested_path) {
        const auto path = std::filesystem::path(std::string(*requested_path));
        if (!std::filesystem::is_directory(path))
            return std::unexpected("Google Drive path is not a directory: " + path.string());
        return domain::DirectoryPath::create(path.string());
    }
    return detail::discover_mount();
}

std::expected<DriveIndexResult, std::string> GoogleDrivePlugin::index(
    std::optional<std::string_view> requested_path) {
    auto directory = resolve(requested_path);
    if (!directory) return std::unexpected(directory.error());
    manifests_.set_source_root(*directory);
    index_.set_cache_root(detail::cache_root_for(*directory));
    auto reconciled = reconcile_.execute_recursive(*directory);
    if (!reconciled) return std::unexpected(reconciled.error());
    auto rebuilt = rebuild_index_.execute(*directory);
    if (!rebuilt) return std::unexpected(rebuilt.error());
    return DriveIndexResult{*directory, *reconciled};
}

std::expected<application::SearchResult, std::string> GoogleDrivePlugin::search(
    const domain::DirectoryPath& directory, std::string_view query, std::size_t limit) {
    manifests_.set_source_root(directory);
    index_.set_cache_root(detail::cache_root_for(directory));
    return search_.execute(directory, query, limit);
}

std::string GoogleDrivePlugin::url_for_item_id(std::string_view item_id) {
    return detail::url_for_item_id(item_id);
}

}  // namespace crumb::plugins::google_drive
