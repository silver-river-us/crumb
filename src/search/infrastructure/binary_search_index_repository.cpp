#include "search/infrastructure/binary_search_index_repository.hpp"

#include "search/infrastructure/binary_search_index_repository/codec.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

namespace crumb::infrastructure {
namespace testing {
CompressFunction compress_function = ::compress2;
}  // namespace testing

namespace {
constexpr std::string_view magic = "CRZ5";
constexpr std::string_view legacy_modified_magic = "CRZ4";
constexpr std::string_view legacy_magic = "CRZ3";
}  // namespace

namespace testing {
std::expected<domain::SearchIndex, std::string> deserialize_for_test(std::istream& input,
                                                                     bool has_modified_ns,
                                                                     bool has_file_id) {
    return detail::deserialize_index(input, has_modified_ns, has_file_id);
}
}  // namespace testing

std::expected<void, std::string> BinarySearchIndexRepository::save(
    const domain::DirectoryPath& root, const domain::SearchIndex& index) {
    const auto path = std::filesystem::path(root.value()) / ".crumb.index";
    const auto temporary = path.string() + ".tmp";
    try {
        std::ostringstream raw(std::ios::binary);
        detail::serialize_index(raw, index);
        const auto input = raw.str();
        uLongf compressed_size = compressBound(static_cast<uLong>(input.size()));
        std::vector<Bytef> compressed(compressed_size);
        if (testing::compress_function(compressed.data(), &compressed_size,
                                       reinterpret_cast<const Bytef*>(input.data()),
                                       static_cast<uLong>(input.size()), Z_BEST_SPEED) != Z_OK)
            return std::unexpected("cannot compress search index");
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        if (!out) return std::unexpected("cannot write " + temporary);
        out.write(magic.data(), static_cast<std::streamsize>(magic.size()));
        const auto raw_size = static_cast<std::uint64_t>(input.size());
        const auto packed_size = static_cast<std::uint64_t>(compressed_size);
        out.write(reinterpret_cast<const char*>(&raw_size), sizeof raw_size);
        out.write(reinterpret_cast<const char*>(&packed_size), sizeof packed_size);
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
    char header[magic.size()]{};
    if (!in.read(header, sizeof header))
        return std::unexpected("invalid search index " + path.string());
    const std::string version(header, sizeof header);
    if (version != magic && version != legacy_modified_magic && version != legacy_magic)
        return std::unexpected("invalid search index " + path.string());
    std::uint64_t raw_size{}, compressed_size{};
    if (!in.read(reinterpret_cast<char*>(&raw_size), sizeof raw_size) ||
        !in.read(reinterpret_cast<char*>(&compressed_size), sizeof compressed_size) ||
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
    return detail::deserialize_index(payload, version != legacy_magic, version == magic);
}
}  // namespace crumb::infrastructure
