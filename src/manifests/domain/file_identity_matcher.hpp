#pragma once

#include "manifests/domain/file_entry.hpp"
#include "files/domain/file_snapshot.hpp"

#include <optional>
#include <vector>

namespace crumb::domain {
class FileIdentityMatcher {
   public:
    [[nodiscard]] static std::optional<std::size_t> match(
        const FileSnapshot& snapshot, const std::vector<FileEntry>& candidates) {
        const auto find_match = [&](auto predicate) -> std::optional<std::size_t> {
            std::optional<std::size_t> found;
            for (std::size_t i = 0; i < candidates.size(); ++i) {
                if (predicate(candidates[i])) {
                    if (found.has_value()) return std::nullopt;
                    found = i;
                }
            }
            return found;
        };
        if (const auto result = find_match([&](const FileEntry& e) {
                return snapshot.metadata.inode && snapshot.metadata.device && e.metadata.inode &&
                       e.metadata.device && *snapshot.metadata.inode == *e.metadata.inode &&
                       *snapshot.metadata.device == *e.metadata.device;
            })) {
            return result;
        }
        if (const auto result = find_match([&](const FileEntry& e) {
                return snapshot.metadata.content_hash && e.metadata.content_hash &&
                       *snapshot.metadata.content_hash == *e.metadata.content_hash &&
                       snapshot.metadata.size == e.metadata.size;
            })) {
            return result;
        }
        return find_match([&](const FileEntry& e) {
            return snapshot.fingerprint.value() == e.fingerprint.value() &&
                   snapshot.metadata.size == e.metadata.size &&
                   snapshot.metadata.modified_ns == e.metadata.modified_ns;
        });
    }
};
}  // namespace crumb::domain
