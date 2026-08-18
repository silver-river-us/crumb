#include "infrastructure/persistence/toml_manifest_mapper.hpp"

#include "infrastructure/persistence/internal/toml_manifest_parser.hpp"

#include <sstream>

namespace crumb::infrastructure {
namespace {
std::string quote(std::string_view value) {
    std::string result = "\"";
    for (const char character : value) {
        if (character == '\\' || character == '"') result += '\\';
        result += character == '\n' ? "\\n" : std::string(1, character);
    }
    return result + '"';
}
}  // namespace

std::expected<domain::DirectoryManifest, std::string> TomlManifestMapper::fromToml(
    std::string_view input) const {
    return fromToml(input, domain::DirectoryPath::create("."));
}

std::expected<domain::DirectoryManifest, std::string> TomlManifestMapper::fromToml(
    std::string_view input, const domain::DirectoryPath& path) const {
    return detail::parse_manifest(input, path);
}

std::string TomlManifestMapper::toToml(const domain::DirectoryManifest& manifest) const {
    std::ostringstream out;
    out << "version = 1\n"
        << "directory_id = " << quote(manifest.id().value()) << "\n"
        << "generated_at = " << quote(manifest.generated_at()) << "\n";
    if (!manifest.generator().empty()) out << "generator = " << quote(manifest.generator()) << "\n";
    for (const auto& file : manifest.files()) {
        out << "\n[files." << quote(file.name.value()) << "]\n"
            << "id = " << quote(file.id.value()) << "\n"
            << "type = " << quote(file.metadata.type) << "\n"
            << "size = " << file.metadata.size << "\n"
            << "modified_ns = " << file.metadata.modified_ns << "\n"
            << "fingerprint = " << quote(file.fingerprint.value()) << "\n";
        if (file.metadata.content_hash)
            out << "content_hash = " << quote(*file.metadata.content_hash) << "\n";
        if (file.metadata.external_url)
            out << "external_url = " << quote(*file.metadata.external_url) << "\n";
        if (file.metadata.created_ns) out << "created_ns = " << *file.metadata.created_ns << "\n";
        if (file.metadata.inode) out << "inode = " << *file.metadata.inode << "\n";
        if (file.metadata.device) out << "device = " << *file.metadata.device << "\n";
        if (file.metadata.title) out << "title = " << quote(*file.metadata.title) << "\n";
        if (file.metadata.author) out << "author = " << quote(*file.metadata.author) << "\n";
        if (file.metadata.extractor)
            out << "extractor = " << quote(*file.metadata.extractor) << "\n";
        for (const auto& [key, value] : file.metadata.extension_fields)
            out << key << " = " << value << "\n";
    }
    return out.str();
}
}  // namespace crumb::infrastructure
