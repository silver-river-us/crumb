#include "infrastructure/persistence/toml_manifest_mapper.hpp"
#include <charconv>
#include <sstream>
#include <unordered_map>

namespace crumb::infrastructure {
namespace {
std::string_view trim(std::string_view value) {
    const auto first = value.find_first_not_of(" \t\r");
    if (first == std::string::npos) return {};
    const auto last = value.find_last_not_of(" \t\r");
    return value.substr(first, last - first + 1);
}
std::string unquote(std::string_view value) {
    value = trim(value);
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        std::string result;
        for (std::size_t i = 1; i + 1 < value.size(); ++i) {
            if (value[i] == '\\' && i + 1 < value.size() - 1) {
                const auto next = value[++i];
                result += next == 'n' ? '\n' : next == 'r' ? '\r' : next == 't' ? '\t' : next;
            } else result += value[i];
        }
        return result;
    }
    return std::string(value);
}
std::string quote(std::string_view value) {
    std::string result = "\"";
    for (const char c : value) { if (c == '\\' || c == '"') result += '\\'; result += c == '\n' ? "\\n" : std::string(1, c); }
    result += '"'; return result;
}
std::optional<std::string> string_field(const std::unordered_map<std::string, std::string>& fields, std::string_view key) {
    if (const auto it = fields.find(std::string(key)); it != fields.end()) return unquote(it->second); return std::nullopt;
}
std::optional<std::uintmax_t> integer_field(const std::unordered_map<std::string, std::string>& fields, std::string_view key) {
    if (const auto value = string_field(fields, key)) { std::uintmax_t n{}; auto [p, e] = std::from_chars(value->data(), value->data() + value->size(), n); if (e == std::errc{}) return n; } return std::nullopt;
}
}
std::expected<domain::DirectoryManifest, std::string> TomlManifestMapper::fromToml(std::string_view input) const {
    return fromToml(input, domain::DirectoryPath::create("."));
}
std::expected<domain::DirectoryManifest, std::string> TomlManifestMapper::fromToml(std::string_view input, const domain::DirectoryPath& path) const {
    std::string section, directory_id, generated_at, generator;
    int version = 0;
    std::unordered_map<std::string, std::string> fields;
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> records;
    auto flush = [&] {
        if (!section.empty() && section.starts_with("files.")) records[unquote(section.substr(6))] = fields;
        fields.clear();
    };
    std::size_t line_start = 0;
    while (line_start < input.size()) {
        const auto line_end = input.find('\n', line_start);
        const auto line = trim(input.substr(line_start, line_end == std::string_view::npos
                                                        ? std::string_view::npos
                                                        : line_end - line_start));
        line_start = line_end == std::string_view::npos ? input.size() : line_end + 1;
        if (line.empty() || line.starts_with('#')) continue;
        if (line.front() == '[' && line.back() == ']') { flush(); section = line.substr(1, line.size() - 2); continue; }
        const auto pos = line.find('='); if (pos == std::string::npos) continue;
        const auto key = trim(line.substr(0, pos)); const auto value = trim(line.substr(pos + 1));
        if (section.empty()) { if (key == "version") { auto parsed = unquote(value); std::from_chars(parsed.data(), parsed.data() + parsed.size(), version); } else if (key == "directory_id") directory_id = unquote(value); else if (key == "generated_at") generated_at = unquote(value); else if (key == "generator") generator = unquote(value); }
        else fields[std::string(key)] = value;
    }
    flush();
    if (version != 1) return std::unexpected("unsupported manifest major version");
    if (directory_id.empty() || generated_at.empty()) return std::unexpected("manifest requires directory_id and generated_at");
    auto manifest = domain::DirectoryManifest::create(domain::DirectoryId::create(directory_id), path, generated_at, generator);
    try {
        (void)manifest;
    }
    catch (const std::exception& e) { return std::unexpected(e.what()); }
    for (const auto& [name, values] : records) {
        try {
            const auto id = string_field(values, "id"); const auto type = string_field(values, "type"); const auto fingerprint = string_field(values, "fingerprint");
            const auto size = integer_field(values, "size"); const auto modified = integer_field(values, "modified_ns");
            if (!id || !type || !fingerprint || !size || !modified) return std::unexpected("file record missing required field: " + name);
            domain::FileMetadata metadata; metadata.type = *type; metadata.size = *size; metadata.modified_ns = static_cast<std::int64_t>(*modified);
            for (const auto& [key, value] : values) {
                if (key == "content_hash") metadata.content_hash = unquote(value); else if (key == "created_ns") { if (auto n = integer_field(values, key)) metadata.created_ns = static_cast<std::int64_t>(*n); } else if (key == "inode") metadata.inode = integer_field(values, key); else if (key == "device") metadata.device = integer_field(values, key); else if (key == "title") metadata.title = unquote(value); else if (key == "author") metadata.author = unquote(value); else if (key == "extractor") metadata.extractor = unquote(value); else if (key != "id" && key != "type" && key != "size" && key != "modified_ns" && key != "fingerprint") metadata.extension_fields[key] = value;
            }
            manifest.add({domain::FileId::create(*id), domain::FileName::create(name), std::move(metadata), domain::Fingerprint::create(*fingerprint)});
        } catch (const std::exception& e) { return std::unexpected(e.what()); }
    }
    return manifest;
}
std::string TomlManifestMapper::toToml(const domain::DirectoryManifest& manifest) const {
    std::ostringstream out;
    out << "version = 1\n" << "directory_id = " << quote(manifest.id().value()) << "\n" << "generated_at = " << quote(manifest.generated_at()) << "\n";
    if (!manifest.generator().empty()) out << "generator = " << quote(manifest.generator()) << "\n";
    for (const auto& file : manifest.files()) {
        out << "\n[files." << quote(file.name.value()) << "]\n";
        out << "id = " << quote(file.id.value()) << "\n" << "type = " << quote(file.metadata.type) << "\n";
        out << "size = " << file.metadata.size << "\n" << "modified_ns = " << file.metadata.modified_ns << "\n";
        out << "fingerprint = " << quote(file.fingerprint.value()) << "\n";
        if (file.metadata.content_hash) out << "content_hash = " << quote(*file.metadata.content_hash) << "\n";
        if (file.metadata.created_ns) out << "created_ns = " << *file.metadata.created_ns << "\n";
        if (file.metadata.inode) out << "inode = " << *file.metadata.inode << "\n";
        if (file.metadata.device) out << "device = " << *file.metadata.device << "\n";
        if (file.metadata.title) out << "title = " << quote(*file.metadata.title) << "\n";
        if (file.metadata.author) out << "author = " << quote(*file.metadata.author) << "\n";
        if (file.metadata.extractor) out << "extractor = " << quote(*file.metadata.extractor) << "\n";
        for (const auto& [key, value] : file.metadata.extension_fields) out << key << " = " << value << "\n";
    }
    return out.str();
}
}
