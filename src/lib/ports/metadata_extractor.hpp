#pragma once

#include "domain/file_metadata.hpp"
#include "domain/value_objects/value_objects.hpp"
#include <expected>
#include <string>

namespace crumb::ports {
class MetadataExtractor {
public:
    virtual ~MetadataExtractor() = default;
    virtual std::expected<domain::FileMetadata, std::string> extract(
        const domain::DirectoryPath&, const domain::FileName&, domain::FileMetadata base) = 0;
};
}
