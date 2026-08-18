#include "manifests/infrastructure/toml_manifest_mapper/parser.hpp"

#include <charconv>
#include <exception>
#include <unordered_map>

namespace crumb::infrastructure::detail {
namespace {
using Fields = std::unordered_map<std::string, std::string>;

std::string_view trim(std::string_view value) {
    const auto first = value.find_first_not_of(" \t\r");
    if (first == std::string::npos) return {};
    const auto last = value.find_last_not_of(" \t\r");
    return value.substr(first, last - first + 1);
}

std::string unquote(std::string_view value) {
    value = trim(value);
    if (value.size() < 2 || value.front() != '"' || value.back() != '"') return std::string(value);
    std::string result;
    for (std::size_t i = 1; i + 1 < value.size(); ++i) {
        if (value[i] == '\\' && i + 1 < value.size() - 1) {
            const auto next = value[++i];
            result += next == 'n' ? '\n' : next == 'r' ? '\r' : next == 't' ? '\t' : next;
        } else
            result += value[i];
    }
    return result;
}

std::optional<std::string> string_field(const Fields& fields, std::string_view key) {
    if (const auto it = fields.find(std::string(key)); it != fields.end())
        return unquote(it->second);
    return std::nullopt;
}

std::optional<std::uintmax_t> integer_field(const Fields& fields, std::string_view key) {
    if (const auto value = string_field(fields, key)) {
        std::uintmax_t number{};
        const auto [pointer, error] =
            std::from_chars(value->data(), value->data() + value->size(), number);
        if (error == std::errc{} && pointer == value->data() + value->size()) return number;
    }
    return std::nullopt;
}

void assign_optional_metadata(domain::FileMetadata& metadata, const Fields& fields,
                              const std::string& key, const std::string& value) {
    if (key == "content_hash")
        metadata.content_hash = unquote(value);
    else if (key == "external_url")
        metadata.external_url = unquote(value);
    else if (key == "created_ns") {
        if (auto number = integer_field(fields, key))
            metadata.created_ns = static_cast<std::int64_t>(*number);
    } else if (key == "inode")
        metadata.inode = integer_field(fields, key);
    else if (key == "device")
        metadata.device = integer_field(fields, key);
    else if (key == "title")
        metadata.title = unquote(value);
    else if (key == "author")
        metadata.author = unquote(value);
    else if (key == "extractor")
        metadata.extractor = unquote(value);
    else if (key != "id" && key != "type" && key != "size" && key != "modified_ns" &&
             key != "fingerprint")
        metadata.extension_fields[key] = value;
}
}  // namespace

std::expected<domain::DirectoryManifest, std::string> parse_manifest(
    std::string_view input, const domain::DirectoryPath& path) {
    std::string section, directory_id, generated_at, generator;
    int version = 0;
    Fields fields;
    std::unordered_map<std::string, Fields> records;
    auto flush = [&] {
        if (section.starts_with("files.")) records[unquote(section.substr(6))] = fields;
        fields.clear();
    };
    for (std::size_t start = 0; start < input.size();) {
        const auto end = input.find('\n', start);
        const auto line = trim(input.substr(
            start, end == std::string_view::npos ? std::string_view::npos : end - start));
        start = end == std::string_view::npos ? input.size() : end + 1;
        if (line.empty() || line.starts_with('#')) continue;
        if (line.front() == '[' && line.back() == ']') {
            flush();
            section = line.substr(1, line.size() - 2);
            continue;
        }
        const auto position = line.find('=');
        if (position == std::string_view::npos) continue;
        const auto key = trim(line.substr(0, position));
        const auto value = trim(line.substr(position + 1));
        if (!section.empty())
            fields[std::string(key)] = value;
        else if (key == "version") {
            const auto parsed = unquote(value);
            std::from_chars(parsed.data(), parsed.data() + parsed.size(), version);
        } else if (key == "directory_id")
            directory_id = unquote(value);
        else if (key == "generated_at")
            generated_at = unquote(value);
        else if (key == "generator")
            generator = unquote(value);
    }
    flush();
    if (version != 1) return std::unexpected("unsupported manifest major version");
    if (directory_id.empty() || generated_at.empty())
        return std::unexpected("manifest requires directory_id and generated_at");
    std::optional<domain::DirectoryManifest> manifest;
    try {
        manifest = domain::DirectoryManifest::create(domain::DirectoryId::create(directory_id),
                                                     path, generated_at, generator);
        for (const auto& [name, values] : records) {
            const auto id = string_field(values, "id");
            const auto type = string_field(values, "type");
            const auto fingerprint = string_field(values, "fingerprint");
            const auto size = integer_field(values, "size");
            const auto modified = integer_field(values, "modified_ns");
            if (!id || !type || !fingerprint || !size || !modified)
                return std::unexpected("file record missing required field: " + name);
            domain::FileMetadata metadata{
                .type = *type, .size = *size, .modified_ns = static_cast<std::int64_t>(*modified)};
            for (const auto& [key, value] : values)
                assign_optional_metadata(metadata, values, key, value);
            manifest->add({domain::FileId::create(*id), domain::FileName::create(name),
                           std::move(metadata), domain::Fingerprint::create(*fingerprint)});
        }
    } catch (const std::exception& error) {
        return std::unexpected(error.what());
    }
    return std::move(*manifest);
}
}  // namespace crumb::infrastructure::detail
