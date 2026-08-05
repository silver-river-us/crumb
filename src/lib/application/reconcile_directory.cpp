#include "lib/application/reconcile_directory.hpp"
#include "domain/directory_manifest.hpp"
#include "domain/file_identity_matcher.hpp"


namespace crumb::application {
std::expected<ReconcileResult, std::string> ReconcileDirectory::execute(const domain::DirectoryPath& directory) {
    auto loaded = manifests_.load(directory);
    if (!loaded) return std::unexpected(loaded.error());
    auto snapshots = filesystem_.list_regular_files(directory);
    if (!snapshots) return std::unexpected(snapshots.error());

    auto manifest = loaded.value().value_or(
        domain::DirectoryManifest::create(ids_.directory_id(), directory, clock_.now_utc(), "crumb/0.1.0"));
    ReconcileResult result;
    std::vector<bool> observed(manifest.files().size(), false);

    for (const auto& snapshot : snapshots.value()) {
        std::size_t matched = manifest.files().size();
        if (const auto* same_name = manifest.find(snapshot.name)) {
            matched = static_cast<std::size_t>(same_name - manifest.files().data());
        } else if (auto candidate = domain::FileIdentityMatcher::match(snapshot, manifest.files())) {
            matched = *candidate;
            manifest.files()[matched].name = snapshot.name;
        }

        const bool existing = matched < manifest.files().size();
        if (existing && manifest.files()[matched].name.value() == snapshot.name.value() &&
            manifest.files()[matched].metadata.size == snapshot.metadata.size &&
            manifest.files()[matched].metadata.modified_ns == snapshot.metadata.modified_ns &&
            manifest.files()[matched].fingerprint.value() == snapshot.fingerprint.value()) {
            observed[matched] = true;
            continue;
        }
        auto metadata = extractor_.extract(directory, snapshot.name, snapshot.metadata);
        if (!metadata) return std::unexpected(metadata.error());
        domain::FileEntry entry;
        if (matched < manifest.files().size()) {
            entry.id = manifest.files()[matched].id;
            entry.name = snapshot.name;
            entry.metadata = std::move(metadata.value());
            entry.fingerprint = snapshot.fingerprint;
            manifest.files()[matched] = std::move(entry);
            observed[matched] = true;
            ++result.updated;
        } else {
            entry = {ids_.file_id(), snapshot.name, std::move(metadata.value()), snapshot.fingerprint};
            manifest.add(std::move(entry));
            observed.push_back(true);
            ++result.added;
        }
    }
    for (std::size_t i = manifest.files().size(); i-- > 0;) {
        if (i >= observed.size() || !observed[i]) { manifest.files().erase(manifest.files().begin() + static_cast<std::ptrdiff_t>(i)); ++result.removed; }
    }
    manifest.set_generated_at(clock_.now_utc());
    auto saved = manifests_.save(manifest);
    if (!saved) return std::unexpected(saved.error());
    return result;
}
}
