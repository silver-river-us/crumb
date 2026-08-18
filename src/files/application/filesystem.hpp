#pragma once

#include "files/domain/file_snapshot.hpp"
#include "files/domain/value_objects/directory_path.hpp"
#include <expected>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace crumb::ports {
class FileSystem {
   public:
    virtual ~FileSystem() = default;
    virtual std::expected<std::vector<domain::FileSnapshot>, std::string> list_regular_files(
        const domain::DirectoryPath& directory) = 0;

    virtual std::expected<std::optional<std::string>, std::string> read_text_file(
        const domain::DirectoryPath& directory, const domain::FileName& name) = 0;
    virtual std::expected<std::vector<std::pair<domain::DirectoryPath, domain::FileName>>,
                          std::string>
    list_regular_files_recursive(const domain::DirectoryPath& directory) = 0;
    virtual std::expected<std::vector<domain::DirectoryPath>, std::string>
    list_directories_recursive(const domain::DirectoryPath& directory) = 0;
};
}  // namespace crumb::ports
