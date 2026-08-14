#pragma once

#include "ports/metadata_extractor.hpp"

#include <expected>
#include <string>

namespace crumb::plugins::google_drive {

class DriveMetadataExtractor final : public ports::MetadataExtractor {
   public:
    DriveMetadataExtractor() = default;

    std::expected<domain::FileMetadata, std::string> extract(const domain::DirectoryPath&,
                                                             const domain::FileName&,
                                                             domain::FileMetadata) override;
};

}  // namespace crumb::plugins::google_drive
