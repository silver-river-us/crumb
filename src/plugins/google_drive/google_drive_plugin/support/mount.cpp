#include "plugins/google_drive/google_drive_plugin/support/details.hpp"

#include <algorithm>
#include <cstdlib>
#include <exception>
#include <iomanip>
#include <sstream>
#include <vector>

namespace crumb::plugins::google_drive::detail {
namespace {
std::filesystem::path cache_base() {
    if (const auto* value = std::getenv("XDG_CACHE_HOME"); value != nullptr && *value != '\0')
        return value;
    if (const auto* home = std::getenv("HOME"); home != nullptr && *home != '\0')
        return std::filesystem::path(home) / ".cache";
    return std::filesystem::temp_directory_path();
}
}  // namespace

std::expected<domain::DirectoryPath, std::string> discover_mount() {
    const auto* home = std::getenv("HOME");
    if (home == nullptr || *home == '\0') return std::unexpected("HOME is not set");
    const auto cloud_storage = std::filesystem::path(home) / "Library" / "CloudStorage";
    std::vector<std::filesystem::path> mounts;
    try {
        for (const auto& entry : std::filesystem::directory_iterator(cloud_storage))
            if (entry.is_directory() &&
                entry.path().filename().string().starts_with("GoogleDrive-"))
                mounts.push_back(entry.path());
    } catch (const std::exception& error) {
        return std::unexpected(error.what());
    }
    std::ranges::sort(mounts);
    if (mounts.empty())
        return std::unexpected("no Google Drive mount found under " + cloud_storage.string());
    if (mounts.size() > 1)
        return std::unexpected("multiple Google Drive mounts found; pass a mount path explicitly");
    return domain::DirectoryPath::create(mounts.front().string());
}

std::filesystem::path cache_root_for(const domain::DirectoryPath& source_root) {
    std::uint64_t hash = 14695981039346656037ull;
    for (const char character : source_root.value()) {
        hash ^= static_cast<unsigned char>(character);
        hash *= 1099511628211ull;
    }
    std::ostringstream key;
    key << std::hex << hash;
    return cache_base() / "crumb" / "google_drive" / key.str();
}

std::string url_for_item_id(std::string_view item_id) {
    return "https://drive.google.com/open?id=" + std::string(item_id);
}
}  // namespace crumb::plugins::google_drive::detail
