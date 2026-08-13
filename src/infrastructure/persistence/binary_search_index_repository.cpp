#include "infrastructure/persistence/binary_search_index_repository.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>
#include <zlib.h>

namespace crumb::infrastructure {
namespace testing {
CompressFunction compress_function = ::compress2;
}  // namespace testing

namespace {
constexpr char magic[] = "CRZ5";
constexpr char legacy_modified_magic[] = "CRZ4";
constexpr char legacy_magic[] = "CRZ3";
template <typename T>
void write_number(std::ostream& out, T value) {
    out.write(reinterpret_cast<const char*>(&value), sizeof value);
}
template <typename T>
bool read_number(std::istream& in, T& value) {
    return static_cast<bool>(in.read(reinterpret_cast<char*>(&value), sizeof value));
}
void write_string(std::ostream& out, const std::string& value) {
    write_number(out, static_cast<std::uint32_t>(value.size()));
    out.write(value.data(), static_cast<std::streamsize>(value.size()));
}
bool read_string(std::istream& in, std::string& value) {
    std::uint32_t size{};
    if (!read_number(in, size) || size > 64 * 1024 * 1024) return false;
    value.resize(size);
    return static_cast<bool>(in.read(value.data(), static_cast<std::streamsize>(size)));
}
void serialize(std::ostream& out, const domain::SearchIndex& index) {
    write_number(out, static_cast<std::uint32_t>(index.documents.size()));
    for (const auto& document : index.documents) {
        write_string(out, document.directory.value());
        write_string(out, document.name.value());
        write_string(out, document.file_id);
        write_number(out, static_cast<std::uint8_t>(document.external_url.has_value()));
        if (document.external_url) write_string(out, *document.external_url);
        write_number(out, static_cast<std::uint8_t>(document.created_ns.has_value()));
        if (document.created_ns) write_number(out, *document.created_ns);
        write_number(out, static_cast<std::uint8_t>(document.modified_ns.has_value()));
        if (document.modified_ns) write_number(out, *document.modified_ns);
    }
    write_number(out, static_cast<std::uint32_t>(index.terms.size()));
    for (const auto& term : index.terms) {
        write_string(out, term.term);
        write_number(out, static_cast<std::uint32_t>(term.postings.size()));
        for (const auto& posting : term.postings) {
            write_number(out, posting.document_id);
            write_number(out, posting.count);
        }
    }
}
std::expected<domain::SearchIndex, std::string> deserialize(std::istream& in, bool has_modified_ns,
                                                            bool has_file_id) {
    domain::SearchIndex index;
    std::uint32_t document_count{}, term_count{};
    if (!read_number(in, document_count) || document_count > 100'000'000)
        return std::unexpected("invalid document count");
    index.documents.reserve(document_count);
    for (std::uint32_t i = 0; i < document_count; ++i) {
        std::string directory, name;
        std::string file_id;
        std::uint8_t has_external_url{};
        if (!read_string(in, directory) || !read_string(in, name) ||
            (has_file_id && !read_string(in, file_id)) || !read_number(in, has_external_url) ||
            has_external_url > 1) {
            return std::unexpected("truncated search index");
        }
        std::optional<std::string> external_url;
        if (has_external_url) {
            std::string value;
            if (!read_string(in, value)) return std::unexpected("truncated search index");
            external_url = std::move(value);
        }
        std::uint8_t has_created_ns{};
        if (!read_number(in, has_created_ns) || has_created_ns > 1)
            return std::unexpected("truncated search index");
        std::optional<std::int64_t> created_ns;
        if (has_created_ns) {
            std::int64_t value{};
            if (!read_number(in, value)) return std::unexpected("truncated search index");
            created_ns = value;
        }
        std::optional<std::int64_t> modified_ns;
        if (has_modified_ns) {
            std::uint8_t has_modified{};
            if (!read_number(in, has_modified) || has_modified > 1)
                return std::unexpected("truncated search index");
            if (has_modified) {
                std::int64_t value{};
                if (!read_number(in, value)) return std::unexpected("truncated search index");
                modified_ns = value;
            }
        }
        try {
            index.documents.push_back({domain::DirectoryPath::create(std::move(directory)),
                                       domain::FileName::create(std::move(name)),
                                       std::move(file_id), std::move(external_url), created_ns,
                                       modified_ns});
        } catch (const std::exception& error) {
            return std::unexpected(error.what());
        }
    }
    if (!read_number(in, term_count) || term_count > 10'000'000)
        return std::unexpected("invalid term count");
    index.terms.reserve(term_count);
    for (std::uint32_t i = 0; i < term_count; ++i) {
        domain::SearchTerm term;
        std::uint32_t posting_count{};
        if (!read_string(in, term.term) || !read_number(in, posting_count) ||
            posting_count > 100'000'000)
            return std::unexpected("truncated search index");
        term.postings.reserve(posting_count);
        for (std::uint32_t j = 0; j < posting_count; ++j) {
            domain::SearchPosting posting;
            if (!read_number(in, posting.document_id) || !read_number(in, posting.count) ||
                posting.document_id >= document_count)
                return std::unexpected("invalid search posting");
            term.postings.push_back(posting);
        }
        index.terms.push_back(std::move(term));
    }
    return index;
}
}  // namespace

namespace testing {
std::expected<domain::SearchIndex, std::string> deserialize_for_test(std::istream& input,
                                                                     bool has_modified_ns,
                                                                     bool has_file_id) {
    return deserialize(input, has_modified_ns, has_file_id);
}
}  // namespace testing

std::expected<void, std::string> BinarySearchIndexRepository::save(
    const domain::DirectoryPath& root, const domain::SearchIndex& index) {
    const auto path = std::filesystem::path(root.value()) / ".crumb.index";
    const auto temporary = path.string() + ".tmp";
    try {
        std::ostringstream raw(std::ios::binary);
        serialize(raw, index);
        const auto input = raw.str();
        uLongf compressed_size = compressBound(static_cast<uLong>(input.size()));
        std::vector<Bytef> compressed(compressed_size);
        if (testing::compress_function(compressed.data(), &compressed_size,
                                       reinterpret_cast<const Bytef*>(input.data()),
                                       static_cast<uLong>(input.size()), Z_BEST_SPEED) != Z_OK)
            return std::unexpected("cannot compress search index");
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        if (!out) return std::unexpected("cannot write " + temporary);
        out.write(magic, sizeof magic - 1);
        write_number(out, static_cast<std::uint64_t>(input.size()));
        write_number(out, static_cast<std::uint64_t>(compressed_size));
        out.write(reinterpret_cast<const char*>(compressed.data()),
                  static_cast<std::streamsize>(compressed_size));
        out.flush();
        if (!out) return std::unexpected("cannot flush " + temporary);
        out.close();
        std::filesystem::rename(temporary, path);
    } catch (const std::exception& error) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        return std::unexpected(error.what());
    }
    return {};
}

std::expected<std::uintmax_t, std::string> BinarySearchIndexRepository::size(
    const domain::DirectoryPath& root) const {
    const auto path = std::filesystem::path(root.value()) / ".crumb.index";
    std::error_code error;
    const auto result = std::filesystem::file_size(path, error);
    if (error) return std::unexpected("cannot inspect " + path.string() + ": " + error.message());
    return result;
}

std::expected<domain::SearchIndex, std::string> BinarySearchIndexRepository::load(
    const domain::DirectoryPath& root) const {
    const auto path = std::filesystem::path(root.value()) / ".crumb.index";
    std::ifstream in(path, std::ios::binary);
    if (!in) return std::unexpected("cannot read " + path.string());
    char header[sizeof magic - 1]{};
    if (!in.read(header, sizeof header))
        return std::unexpected("invalid search index " + path.string());
    const std::string version(header, sizeof header);
    if (version != magic && version != legacy_modified_magic && version != legacy_magic) {
        return std::unexpected("invalid search index " + path.string());
    }
    std::uint64_t raw_size{}, compressed_size{};
    if (!read_number(in, raw_size) || !read_number(in, compressed_size) ||
        raw_size > 512ULL * 1024 * 1024 || compressed_size > 512ULL * 1024 * 1024)
        return std::unexpected("invalid search index sizes");
    std::vector<Bytef> compressed(compressed_size);
    if (!in.read(reinterpret_cast<char*>(compressed.data()),
                 static_cast<std::streamsize>(compressed_size)))
        return std::unexpected("truncated search index");
    std::string raw(raw_size, '\0');
    uLongf output_size = static_cast<uLongf>(raw_size);
    if (uncompress(reinterpret_cast<Bytef*>(raw.data()), &output_size, compressed.data(),
                   static_cast<uLong>(compressed_size)) != Z_OK ||
        output_size != raw_size)
        return std::unexpected("cannot decompress search index");
    std::istringstream payload(raw, std::ios::binary);
    return deserialize(payload, version != legacy_magic, version == magic);
}
}  // namespace crumb::infrastructure
