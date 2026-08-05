#pragma once

#include "domain/file_snapshot.hpp"
#include <expected>
#include <string>
#include <vector>

namespace crumb::ports {
class FileSystem {
public:
    virtual ~FileSystem() = default;
    virtual std::expected<std::vector<domain::FileSnapshot>, std::string>
    list_regular_files(const domain::DirectoryPath& directory) = 0;
    virtual std::expected<void, std::string> move_file(
        const domain::DirectoryPath& source, const domain::FileName& old_name,
        const domain::DirectoryPath& destination, const domain::FileName& new_name) = 0;
};
}
