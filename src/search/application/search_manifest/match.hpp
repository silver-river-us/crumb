#pragma once

#include "files/domain/value_objects/directory_path.hpp"
#include "files/domain/value_objects/file_name.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace crumb::application {

struct SearchMatch {
    domain::DirectoryPath directory;
    domain::FileName name;
    double score{};
    std::string type;
    std::optional<std::string> external_url;
    std::optional<std::string> title;
    std::optional<std::string> author;
    std::optional<std::int64_t> created_ns;
    std::optional<std::int64_t> modified_ns;
    std::optional<std::string> file_id;
};

}  // namespace crumb::application
