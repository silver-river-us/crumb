#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace crumb::domain {

struct FileMetadata {
    std::string type{"application/octet_stream"};
    std::uintmax_t size{};
    std::int64_t modified_ns{};
    std::optional<std::int64_t> created_ns;
    std::optional<std::uintmax_t> inode;
    std::optional<std::uintmax_t> device;
    std::optional<std::string> content_hash;
    std::optional<std::string> external_url;
    std::optional<std::string> title;
    std::optional<std::string> author;
    std::vector<std::string> tags;
    std::optional<std::string> extractor;
    std::map<std::string, std::string> extension_fields;
};
}
