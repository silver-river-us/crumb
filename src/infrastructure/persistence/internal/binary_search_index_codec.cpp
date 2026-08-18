#include "infrastructure/persistence/internal/binary_search_index_codec.hpp"

#include <cstdint>
#include <exception>
#include <istream>
#include <ostream>
#include <utility>

namespace crumb::infrastructure::detail {
namespace {
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
}  // namespace

void serialize_index(std::ostream& out, const domain::SearchIndex& index) {
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

std::expected<domain::SearchIndex, std::string> deserialize_index(std::istream& in,
                                                                  bool has_modified_ns,
                                                                  bool has_file_id) {
    domain::SearchIndex index;
    std::uint32_t document_count{}, term_count{};
    if (!read_number(in, document_count) || document_count > 100'000'000)
        return std::unexpected("invalid document count");
    index.documents.reserve(document_count);
    for (std::uint32_t i = 0; i < document_count; ++i) {
        std::string directory, name, file_id;
        std::uint8_t has_external_url{};
        if (!read_string(in, directory) || !read_string(in, name) ||
            (has_file_id && !read_string(in, file_id)) || !read_number(in, has_external_url) ||
            has_external_url > 1)
            return std::unexpected("truncated search index");
        std::optional<std::string> external_url;
        if (has_external_url) {
            std::string value;
            if (!read_string(in, value)) return std::unexpected("truncated search index");
            external_url = std::move(value);
        }
        std::optional<std::int64_t> created_ns, modified_ns;
        std::uint8_t has_created_ns{};
        if (!read_number(in, has_created_ns) || has_created_ns > 1)
            return std::unexpected("truncated search index");
        if (has_created_ns) {
            std::int64_t value{};
            if (!read_number(in, value)) return std::unexpected("truncated search index");
            created_ns = value;
        }
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
}  // namespace crumb::infrastructure::detail
