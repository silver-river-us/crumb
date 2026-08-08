#include "lib/application/reconcile_directory.hpp"
#include "domain/directory_manifest.hpp"
#include "domain/file_identity_matcher.hpp"

#include <cctype>
#include <map>

namespace crumb::application {
namespace {
void add_terms(std::map<std::string, std::uint32_t>& terms, std::string_view text) {
    std::string current;
    const auto add = [&terms](std::string word) {
        if (word.size() >= 3) ++terms[word];
    };
    for (const unsigned char character : text) {
        if (std::isalnum(character) || character == '_') current.push_back(static_cast<char>(std::tolower(character)));
        else if (!current.empty()) { add(std::move(current)); current.clear(); }
    }
    if (!current.empty()) add(std::move(current));
}
void add_entry_terms(std::map<std::string, std::uint32_t>& terms, const domain::FileEntry& entry) {
    add_terms(terms, entry.name.value());
    add_terms(terms, entry.metadata.type);
    if (entry.metadata.title) add_terms(terms, *entry.metadata.title);
    if (entry.metadata.author) add_terms(terms, *entry.metadata.author);
    for (const auto& tag : entry.metadata.tags) add_terms(terms, tag);
    for (const auto& [key, value] : entry.metadata.extension_fields) add_terms(terms, key + " " + value);
}
}
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
        ++result.scanned;
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
            manifest.files()[matched].fingerprint.value() == snapshot.fingerprint.value() &&
            manifest.files()[matched].metadata.extension_fields.contains("crumb.search_terms_v2")) {
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
    if (index_) {
        std::map<std::string, std::map<std::uint32_t, std::uint32_t>> inverted;

        ports::SearchIndex persisted;
        for (const auto& current : directories.value()) {
            auto loaded = manifests_.load(current);
            if (!loaded) return std::unexpected(loaded.error());
            if (!loaded.value()) continue;
            for (const auto& entry : loaded.value()->files()) {
                const auto document_id = static_cast<std::uint32_t>(persisted.documents.size());
                persisted.documents.push_back({current, entry.name});

                std::map<std::string, std::uint32_t> terms;
                add_entry_terms(terms, entry);
                for (const auto& [term, count] : terms) inverted[term][document_id] = count;
            }
        }
        for (auto& [term, postings] : inverted) {
            ports::SearchTerm item{std::move(term), {}};
            for (const auto& [document_id, count] : postings) item.postings.push_back({document_id, count});
            persisted.terms.push_back(std::move(item));
        }
        auto saved = index_->save(directory, persisted);
        if (!saved) return std::unexpected(saved.error());
    }
    return total;
}
}
