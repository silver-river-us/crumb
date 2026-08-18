#pragma once

#include "manifests/application/reconcile_directory/result.hpp"
#include "files/domain/value_objects/directory_path.hpp"

#include <expected>
#include <string>

namespace crumb::ports {
class Clock;
class FileSystem;
class FingerprintService;
class IdGenerator;
class ManifestRepository;
class MetadataExtractor;
}  // namespace crumb::ports

namespace crumb::application {

class ReconcileDirectory {
   public:
    ReconcileDirectory(ports::ManifestRepository& manifests, ports::FileSystem& filesystem,
                       ports::FingerprintService&, ports::MetadataExtractor& extractor,
                       ports::IdGenerator& ids, ports::Clock& clock)
        : manifests_(manifests),
          filesystem_(filesystem),
          extractor_(extractor),
          ids_(ids),
          clock_(clock) {}
    std::expected<ReconcileResult, std::string> execute(const domain::DirectoryPath& directory);
    std::expected<ReconcileResult, std::string> execute_recursive(
        const domain::DirectoryPath& directory);

   private:
    ports::ManifestRepository& manifests_;
    ports::FileSystem& filesystem_;
    ports::MetadataExtractor& extractor_;
    ports::IdGenerator& ids_;
    ports::Clock& clock_;
};

}  // namespace crumb::application
