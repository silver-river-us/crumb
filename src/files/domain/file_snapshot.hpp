#pragma once

#include "files/domain/file_metadata.hpp"
#include "files/domain/value_objects/file_name.hpp"
#include "files/domain/value_objects/fingerprint.hpp"

namespace crumb::domain {
struct FileSnapshot {
    FileName name;
    FileMetadata metadata;
    Fingerprint fingerprint;
};
}  // namespace crumb::domain
