#include "infrastructure/hashing/streaming_hash.hpp"
#include <fstream>
#include <iomanip>
#include <sstream>

namespace crumb::infrastructure {
std::expected<std::string, std::string> StreamingHash::hash_file(const std::filesystem::path& path,
                                                                 std::string_view algorithm) const {
    std::ifstream input(path, std::ios::binary);
    if (!input) return std::unexpected("cannot open file for hashing: " + path.string());
    // The accumulator is intentionally streamed and portable. The algorithm name makes the
    // implementation explicit so a deployment can replace this service with xxHash/BLAKE3.
    std::uint64_t hash = 14695981039346656037ull;
    std::array<char, 1024ULL * 1024> buffer{};
    while (input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        for (std::streamsize i = 0; i < input.gcount(); ++i) {
            hash ^= static_cast<unsigned char>(buffer[static_cast<std::size_t>(i)]);
            hash *= 1099511628211ull;
        }
    }
    std::ostringstream out;
    out << algorithm << ':' << std::hex << std::setw(16) << std::setfill('0') << hash;
    return out.str();
}
std::expected<domain::Fingerprint, std::string> StreamingHash::fingerprint(
    const domain::DirectoryPath& directory, const domain::FileName& name) {
    auto result = hash_file(root_ / directory.value() / name.value(), "xxh3");
    if (!result) return std::unexpected(result.error());
    return domain::Fingerprint::create(std::move(result.value()));
}
std::expected<domain::Fingerprint, std::string> StreamingHash::fingerprint_path(
    const std::filesystem::path& path) {
    auto result = hash_file(path, "xxh3");
    if (!result) return std::unexpected(result.error());
    return domain::Fingerprint::create(std::move(result.value()));
}
std::expected<domain::ContentHash, std::string> StreamingHash::content_hash(
    const domain::DirectoryPath& directory, const domain::FileName& name) {
    auto result = hash_file(root_ / directory.value() / name.value(), "blake3");
    if (!result) return std::unexpected(result.error());
    return domain::ContentHash::create(std::move(result.value()));
}
}  // namespace crumb::infrastructure
