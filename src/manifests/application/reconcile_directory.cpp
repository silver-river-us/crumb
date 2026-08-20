#include <vector>
#include <cstddef>
#include <expected>
#include <map>
#include <optional>
#include <string>
#include <utility>

#include "manifests/application/reconcile_directory/reconcile.hpp"
#include "manifests/domain/directory_manifest.hpp"
#include "manifests/domain/file_identity_matcher.hpp"
#include "files/application/clock.hpp"
#include "files/application/filesystem.hpp"
#include "files/application/id_generator.hpp"
#include "manifests/application/manifest_repository.hpp"
#include "files/application/metadata_extractor.hpp"
#include "files/domain/file_metadata.hpp"
#include "files/domain/file_snapshot.hpp"
#include "files/domain/value_objects/directory_path.hpp"
#include "files/domain/value_objects/file_id.hpp"
#include "files/domain/value_objects/file_name.hpp"
#include "files/domain/value_objects/fingerprint.hpp"
#include "manifests/application/reconcile_directory/result.hpp"
#include "manifests/domain/file_entry.hpp"

namespace crumb::application {
namespace {
void preserve_custom_metadata(const domain::FileMetadata& previous,
                              domain::FileMetadata& refreshed) {
    for (const auto& [key, value] : previous.extension_fields) {
        // crumb.* fields are maintained by extractors; all other extension
        // fields are user metadata and must survive a filesystem reindex.
        if (!key.starts_with("crumb.") && !refreshed.extension_fields.contains(key)) {
            refreshed.extension_fields.emplace(key, value);
        }
    }
}
}  // namespace

std::expected<ReconcileResult, std::string> ReconcileDirectory::execute(
    const domain::DirectoryPath& directory) {
    auto loaded = manifests_.load(directory);
    if (!loaded) return std::unexpected(loaded.error());
    auto snapshots = filesystem_.list_regular_files(directory);
    if (!snapshots) return std::unexpected(snapshots.error());

    auto manifest = loaded.value().value_or(domain::DirectoryManifest::create(
        ids_.directory_id(), directory, clock_.now_utc(), "crumb/0.1.0"));
    ReconcileResult result;
    std::vector<bool> observed(manifest.files().size(), false);

    for (const auto& snapshot : snapshots.value()) {
        ++result.scanned;
        std::size_t matched = manifest.files().size();
        if (const auto* same_name = manifest.find(snapshot.name)) {
            matched = static_cast<std::size_t>(same_name - manifest.files().data());
        } else if (auto candidate =
                       domain::FileIdentityMatcher::match(snapshot, manifest.files())) {
            matched = *candidate;
            manifest.files()[matched].name = snapshot.name;
        }

        const bool existing = matched < manifest.files().size();
        if (existing && manifest.files()[matched].name.value() == snapshot.name.value() &&
            manifest.files()[matched].metadata.size == snapshot.metadata.size &&
            manifest.files()[matched].metadata.modified_ns == snapshot.metadata.modified_ns &&
            manifest.files()[matched].metadata.created_ns == snapshot.metadata.created_ns &&
            manifest.files()[matched].fingerprint.value() == snapshot.fingerprint.value() &&
            manifest.files()[matched].metadata.external_url == snapshot.metadata.external_url &&
            manifest.files()[matched].metadata.extension_fields.contains("crumb.search_terms_v3")) {
            observed[matched] = true;
            continue;
        }
        auto metadata = extractor_.extract(directory, snapshot.name, snapshot.metadata);
        if (!metadata) return std::unexpected(metadata.error());
        domain::FileEntry entry;
        if (matched < manifest.files().size()) {
            preserve_custom_metadata(manifest.files()[matched].metadata, metadata.value());
            entry.id = manifest.files()[matched].id;
            entry.name = snapshot.name;
            entry.metadata = std::move(metadata.value());
            entry.fingerprint = snapshot.fingerprint;
            manifest.files()[matched] = std::move(entry);
            observed[matched] = true;
            ++result.updated;
        } else {
            entry = {ids_.file_id(), snapshot.name, std::move(metadata.value()),
                     snapshot.fingerprint};
            manifest.add(std::move(entry));
            observed.push_back(true);
            ++result.added;
        }
    }
    for (std::size_t i = manifest.files().size(); i-- > 0;) {
        if (i >= observed.size() || !observed[i]) {
            manifest.files().erase(manifest.files().begin() + static_cast<std::ptrdiff_t>(i));
            ++result.removed;
        }
    }
    manifest.set_generated_at(clock_.now_utc());
    auto saved = manifests_.save(manifest);
    if (!saved) return std::unexpected(saved.error());
    return result;
}

std::expected<ReconcileResult, std::string> ReconcileDirectory::execute_recursive(
    const domain::DirectoryPath& directory) {
    auto directories = filesystem_.list_directories_recursive(directory);
    if (!directories) return std::unexpected(directories.error());

    ReconcileResult total;
    for (const auto& current : directories.value()) {
        auto result = execute(current);
        if (!result) return std::unexpected(result.error());
        total.scanned += result->scanned;
        total.added += result->added;
        total.updated += result->updated;
        total.removed += result->removed;
    }
    return total;
}
}  // namespace crumb::application
