#include "infrastructure/persistence/toml_manifest_repository.hpp"
#include <filesystem>
#include <fstream>

namespace crumb::infrastructure {
std::expected<std::optional<domain::DirectoryManifest>, std::string> TomlManifestRepository::load(const domain::DirectoryPath& directory) {
    const auto path = std::filesystem::path(directory.value()) / ".crumb";
    if (!std::filesystem::exists(path)) return std::nullopt;
    std::ifstream input(path, std::ios::binary);
    if (!input) return std::unexpected("cannot read " + path.string());
    const std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    auto manifest = mapper_.fromToml(text, directory);
    if (!manifest) return std::unexpected(manifest.error());
    return std::optional<domain::DirectoryManifest>(std::move(manifest.value()));
}
std::expected<void, std::string> TomlManifestRepository::save(const domain::DirectoryManifest& manifest) {
    const auto directory = std::filesystem::path(manifest.path().value());
    const auto temporary = directory / ".crumb.tmp";
    const auto target = directory / ".crumb";
    try {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) return std::unexpected("cannot write " + temporary.string());
        output << mapper_.toToml(manifest);
        output.flush();
        if (!output) return std::unexpected("cannot flush " + temporary.string());
        output.close();
        std::filesystem::rename(temporary, target);
    } catch (const std::exception& error) {
        std::error_code ignored; std::filesystem::remove(temporary, ignored);
        return std::unexpected(error.what());
    }
    return {};
}
}
