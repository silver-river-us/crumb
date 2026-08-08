#pragma once

#include "domain/value_objects/value_objects.hpp"
#include "lib/ports/clock.hpp"
#include "lib/ports/filesystem.hpp"
#include "lib/ports/fingerprint_service.hpp"
#include "lib/ports/id_generator.hpp"
#include "lib/ports/manifest_repository.hpp"
#include "lib/ports/metadata_extractor.hpp"
#include <cstddef>
#include <expected>
#include <string>

namespace crumb::application {
struct ReconcileResult {
    std::size_t scanned{};
    std::size_t added{};
    std::size_t updated{};
    std::size_t removed{};
};
class ReconcileDirectory {
public:
    ReconcileDirectory(ports::ManifestRepository& manifests, ports::FileSystem& filesystem,
                       ports::FingerprintService& fingerprints, ports::MetadataExtractor& extractor,
                       ports::IdGenerator& ids, ports::Clock& clock)
        : manifests_(manifests), filesystem_(filesystem), fingerprints_(fingerprints), extractor_(extractor), ids_(ids), clock_(clock) {}
    std::expected<ReconcileResult, std::string> execute(const domain::DirectoryPath& directory);
    std::expected<ReconcileResult, std::string> execute_recursive(const domain::DirectoryPath& directory);
private:
    ports::ManifestRepository& manifests_;
    ports::FileSystem& filesystem_;
    ports::FingerprintService& fingerprints_;
    ports::MetadataExtractor& extractor_;
    ports::IdGenerator& ids_;
    ports::Clock& clock_;
};
}
