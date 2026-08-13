#pragma once

#include "domain/file_metadata.hpp"
#include "domain/value_objects/directory_path.hpp"
#include "domain/value_objects/file_name.hpp"
#include <expected>
#include <string>

namespace crumb::ports {
class MetadataExtractor {
   public:
    virtual ~MetadataExtractor() = default;
    virtual std::expected<domain::FileMetadata, std::string> extract(const domain::DirectoryPath&,
                                                                     const domain::FileName&,
                                                                     domain::FileMetadata base) = 0;
};
}  // namespace crumb::ports
