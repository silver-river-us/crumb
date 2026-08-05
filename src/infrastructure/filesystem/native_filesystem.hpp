#pragma once
#include "lib/ports/filesystem.hpp"
#include "lib/ports/metadata_extractor.hpp"
namespace crumb::infrastructure {
class NativeFileSystem final : public ports::FileSystem, public ports::MetadataExtractor {
public:
    std::expected<std::vector<domain::FileSnapshot>, std::string> list_regular_files(const domain::DirectoryPath&) override;
    std::expected<void, std::string> move_file(const domain::DirectoryPath&, const domain::FileName&,
                                               const domain::DirectoryPath&, const domain::FileName&) override;
    std::expected<domain::FileMetadata, std::string> extract(const domain::DirectoryPath&, const domain::FileName&, domain::FileMetadata) override;
};
}
