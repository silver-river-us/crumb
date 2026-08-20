#pragma once
#include <string>

#include "files/application/filesystem.hpp"
#include "files/application/metadata_extractor.hpp"

namespace crumb::infrastructure {
class NativeFileSystem final : public ports::FileSystem, public ports::MetadataExtractor {
   public:
    std::expected<std::vector<domain::FileSnapshot>, std::string> list_regular_files(
        const domain::DirectoryPath&) override;

    std::expected<domain::FileMetadata, std::string> extract(const domain::DirectoryPath&,
                                                             const domain::FileName&,
                                                             domain::FileMetadata) override;
    std::expected<std::optional<std::string>, std::string> read_text_file(
        const domain::DirectoryPath&, const domain::FileName&) override;
    std::expected<std::vector<std::pair<domain::DirectoryPath, domain::FileName>>, std::string>
    list_regular_files_recursive(const domain::DirectoryPath&) override;
    std::expected<std::vector<domain::DirectoryPath>, std::string> list_directories_recursive(
        const domain::DirectoryPath&) override;
};
}  // namespace crumb::infrastructure
