#pragma once

#include "files/domain/value_objects/content_hash.hpp"
#include "files/domain/value_objects/directory_path.hpp"
#include "files/domain/value_objects/file_name.hpp"
#include "files/domain/value_objects/fingerprint.hpp"
#include <expected>
#include <string>

namespace crumb::ports {
class FingerprintService {
   public:
    virtual ~FingerprintService() = default;
    virtual std::expected<domain::Fingerprint, std::string> fingerprint(
        const domain::DirectoryPath&, const domain::FileName&) = 0;
    virtual std::expected<domain::ContentHash, std::string> content_hash(
        const domain::DirectoryPath&, const domain::FileName&) = 0;
};
}  // namespace crumb::ports
