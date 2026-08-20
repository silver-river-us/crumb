#include "google_drive/infrastructure/google_drive_plugin/drive_search_index_repository.hpp"

#include <exception>
#include <expected>

#include "files/domain/value_objects/directory_path.hpp"
#include "search/domain/search_index/index.hpp"

namespace crumb::plugins::google_drive {

std::expected<void, std::string> DriveSearchIndexRepository::save(
    const domain::DirectoryPath&, const domain::SearchIndex& index) {
    if (cache_root_.empty()) return std::unexpected("Drive storage root is not configured");
    try {
        std::filesystem::create_directories(cache_root_);
    } catch (const std::exception& error) {
        return std::unexpected(error.what());
    }
    return delegate_.save(domain::DirectoryPath::create(cache_root_.string()), index);
}

std::expected<domain::SearchIndex, std::string> DriveSearchIndexRepository::load(
    const domain::DirectoryPath&) const {
    if (cache_root_.empty()) return std::unexpected("Drive storage root is not configured");
    return delegate_.load(domain::DirectoryPath::create(cache_root_.string()));
}

std::expected<std::uintmax_t, std::string> DriveSearchIndexRepository::size(
    const domain::DirectoryPath&) const {
    if (cache_root_.empty()) return std::unexpected("Drive storage root is not configured");
    return delegate_.size(domain::DirectoryPath::create(cache_root_.string()));
}

}  // namespace crumb::plugins::google_drive
