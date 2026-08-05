#pragma once
#include "domain/directory_manifest.hpp"
#include <expected>
#include <string_view>
namespace crumb::infrastructure {
class TomlManifestMapper {
public:
    std::expected<domain::DirectoryManifest, std::string> fromToml(std::string_view input) const;
    std::expected<domain::DirectoryManifest, std::string> fromToml(std::string_view input, const domain::DirectoryPath& path) const;
    std::string toToml(const domain::DirectoryManifest& manifest) const;
};
}
