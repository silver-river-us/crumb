#pragma once
#include <filesystem>
#include <expected>
#include <string>
#include <string_view>
#include <utility>

#include "files/application/fingerprint_service.hpp"
#include "files/domain/value_objects/fingerprint.hpp"

namespace crumb::infrastructure {
class StreamingHash final : public ports::FingerprintService {
   public:
    explicit StreamingHash(std::filesystem::path root) : root_(std::move(root)) {}
    std::expected<domain::Fingerprint, std::string> fingerprint_path(
        const std::filesystem::path& path);
    std::expected<domain::Fingerprint, std::string> fingerprint(const domain::DirectoryPath&,
                                                                const domain::FileName&) override;
    std::expected<domain::ContentHash, std::string> content_hash(const domain::DirectoryPath&,
                                                                 const domain::FileName&) override;

   private:
    std::filesystem::path root_;
    std::expected<std::string, std::string> hash_file(const std::filesystem::path&,
                                                      std::string_view) const;
};
}  // namespace crumb::infrastructure
