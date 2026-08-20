#pragma once

#include <expected>
#include <string>

namespace crumb::domain {
class DirectoryPath;
}
namespace crumb::ports {
class FileSystem;
class ManifestRepository;
class SearchIndexRepository;
}  // namespace crumb::ports

namespace crumb::application {

class RebuildSearchIndex {
   public:
    RebuildSearchIndex(ports::ManifestRepository& manifests, ports::FileSystem& filesystem,
                       ports::SearchIndexRepository& index)
        : manifests_(manifests), filesystem_(filesystem), index_(index) {}

    [[nodiscard]] std::expected<void, std::string> execute(const domain::DirectoryPath& directory);

   private:
    ports::ManifestRepository& manifests_;
    ports::FileSystem& filesystem_;
    ports::SearchIndexRepository& index_;
};

}  // namespace crumb::application
