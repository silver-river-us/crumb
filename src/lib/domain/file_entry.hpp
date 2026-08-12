#pragma once

#include "domain/file_metadata.hpp"
#include "domain/value_objects/value_objects.hpp"

namespace crumb::domain {
struct FileEntry {
    FileId id;
    FileName name;
    FileMetadata metadata;
    Fingerprint fingerprint;
};
}  // namespace crumb::domain
