#pragma once

#include "domain/value_objects/value_objects.hpp"
#include <expected>
#include <string>

namespace crumb::ports {
class FingerprintService {
public:
    virtual ~FingerprintService() = default;
    virtual std::expected<domain::Fingerprint, std::string> fingerprint(const domain::DirectoryPath&, const domain::FileName&) = 0;
    virtual std::expected<domain::ContentHash, std::string> content_hash(const domain::DirectoryPath&, const domain::FileName&) = 0;
};
}
