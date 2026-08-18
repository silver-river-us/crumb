#include "plugins/google_drive/google_drive_plugin/drive_metadata_extractor.hpp"

#include "plugins/google_drive/google_drive_plugin/support/details.hpp"

#include <filesystem>
#include <utility>

namespace crumb::plugins::google_drive {

std::expected<domain::FileMetadata, std::string> DriveMetadataExtractor::extract(
    const domain::DirectoryPath& directory, const domain::FileName& name,
    domain::FileMetadata base) {
    auto metadata = std::move(base);
    const auto dot = name.value().rfind('.');
    metadata.title = name.value().substr(0, dot);
    const auto path = std::filesystem::path(directory.value()) / name.value();
    if (const auto item_id = detail::read_item_id(path);
        item_id && detail::is_drive_item_id(*item_id))
        metadata.external_url = detail::url_for_item_id(*item_id);
    auto content = detail::extract_plain_text(path);
    if (!content) content = detail::extract_office_text(path);
    if (content)
        metadata.extension_fields["crumb.search_terms_v3"] = detail::search_terms(*content);
    return metadata;
}

}  // namespace crumb::plugins::google_drive
