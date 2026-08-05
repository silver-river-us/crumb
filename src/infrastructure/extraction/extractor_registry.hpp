#pragma once
#include "lib/ports/metadata_extractor.hpp"
namespace crumb::infrastructure { class ExtractorRegistry final : public ports::MetadataExtractor {
public: std::expected<domain::FileMetadata, std::string> extract(const domain::DirectoryPath&, const domain::FileName&, domain::FileMetadata base) override { return base; }
}; }
