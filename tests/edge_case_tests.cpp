#include <cassert>
#include <optional>
#include <string>
#include <cstdint>
#include <expected>
#include <initializer_list>
#include <map>
#include <string_view>
#include <utility>
#include <vector>

#include "search/application/rebuild_search_index.hpp"
#include "search/application/search_manifest/search.hpp"
#include "manifests/domain/directory_manifest.hpp"
#include "files/domain/value_objects/content_hash.hpp"
#include "files/domain/value_objects/directory_id.hpp"
#include "files/domain/value_objects/file_id.hpp"
#include "files/domain/value_objects/fingerprint.hpp"
#include "files/domain/value_objects/value_objects.hpp"
#include "manifests/domain/file_identity_matcher.hpp"
#include "files/application/filesystem.hpp"
#include "manifests/application/manifest_repository.hpp"
#include "search/application/search_index_repository.hpp"
#include "files/domain/file_metadata.hpp"
#include "files/domain/file_snapshot.hpp"
#include "files/domain/value_objects/directory_path.hpp"
#include "files/domain/value_objects/file_name.hpp"
#include "files/domain/value_objects/validated_string.hpp"
#include "manifests/domain/file_entry.hpp"
#include "search/application/search_manifest/match.hpp"
#include "search/application/search_manifest/result.hpp"
#include "search/domain/search_index/index.hpp"

namespace {
using namespace crumb;
const auto directory_id = domain::DirectoryId::create("01K1AB5YZ4QH7M2D8E3F9G6JNX");
const auto file_id = domain::FileId::create("01K1ADN1ZC5R7H4XB8QKMP2TV6");

template <typename Function>
void throws(Function&& function) {
    bool caught = false;
    try {
        function();
    } catch (...) {
        caught = true;
    }
    assert(caught);
}

domain::FileEntry entry(std::string name, const domain::FileId& id = file_id) {
    domain::FileMetadata metadata;
    metadata.size = 3;
    metadata.modified_ns = 4;
    return {id, domain::FileName::create(std::move(name)), metadata,
            domain::Fingerprint::create("test:fingerprint")};
}

void value_and_manifest_tests() {
    const domain::ValidatedString empty_value;
    assert(empty_value.empty());
    for (const std::string value : {"", ".", "..", "a/b", "a\\b"})
        throws([&] { (void)domain::FileName::create(value); });
    throws([] { (void)domain::DirectoryPath::create(""); });
    throws([] { (void)domain::DirectoryId::create("short"); });
    throws([] { (void)domain::FileId::create("short"); });
    throws([] { (void)domain::Fingerprint::create("missing"); });
    throws([] { (void)domain::ContentHash::create("missing"); });
    assert(domain::file_id_hash(file_id).starts_with("fid:"));

    auto manifest =
        domain::DirectoryManifest::create(directory_id, domain::DirectoryPath::create("."));
    manifest.add(entry("b"));
    manifest.add(entry("a", domain::FileId::create("01K1AC4K3N7JZM5F21V6PH8QRT")));
    assert(manifest.files().front().name.value() == "a");
    throws([&] { manifest.add(entry("a")); });
    throws([&] { manifest.remove(domain::FileName::create("missing")); });
    throws([&] {
        manifest.rename(domain::FileName::create("missing"), domain::FileName::create("x"));
    });
    throws([&] { manifest.rename(domain::FileName::create("a"), domain::FileName::create("b")); });
    throws([&] { manifest.update(entry("missing")); });
    throws([&] { manifest.update(entry("a")); });
    manifest.update(entry("a", domain::FileId::create("01K1AC4K3N7JZM5F21V6PH8QRT")));
    manifest.rename(domain::FileName::create("a"), domain::FileName::create("a"));
    manifest.remove(domain::FileName::create("b"));
    manifest.set_generated_at("later");
    manifest.set_generator("generator");
    assert(manifest.find(domain::FileName::create("b")) == nullptr);
    const auto& const_manifest = manifest;
    assert(const_manifest.find(domain::FileName::create("b")) == nullptr);
}

void identity_tests() {
    auto candidate = entry("same");
    domain::FileSnapshot snapshot{domain::FileName::create("new"), candidate.metadata,
                                  candidate.fingerprint};
    assert(domain::FileIdentityMatcher::match(snapshot, {candidate}) == 0);
    auto inode = candidate;
    inode.metadata.inode = 1;
    inode.metadata.device = 2;
    snapshot.metadata.inode = 1;
    snapshot.metadata.device = 2;
    assert(domain::FileIdentityMatcher::match(snapshot, {inode}) == 0);
    auto second = inode;
    second.id = domain::FileId::create("01K1AC4K3N7JZM5F21V6PH8QRT");
    assert(!domain::FileIdentityMatcher::match(snapshot, {inode, second}));
    snapshot.metadata.inode.reset();
    snapshot.metadata.device.reset();
    inode.metadata.inode.reset();
    inode.metadata.device.reset();
    snapshot.metadata.content_hash = "blake3:same";
    inode.metadata.content_hash = "blake3:same";
    assert(domain::FileIdentityMatcher::match(snapshot, {inode}) == 0);
    snapshot.metadata.content_hash.reset();
    inode.metadata.content_hash.reset();
    assert(domain::FileIdentityMatcher::match(snapshot, {inode}) == 0);
    snapshot.metadata.size++;
    assert(!domain::FileIdentityMatcher::match(snapshot, {inode}));
}

class Filesystem final : public ports::FileSystem {
   public:
    bool directory_error = false;
    std::vector<domain::DirectoryPath> directories;
    std::expected<std::vector<domain::FileSnapshot>, std::string> list_regular_files(
        const domain::DirectoryPath&) override {
        return std::vector<domain::FileSnapshot>{};
    }
    std::expected<std::optional<std::string>, std::string> read_text_file(
        const domain::DirectoryPath&, const domain::FileName&) override {
        return std::nullopt;
    }
    std::expected<std::vector<std::pair<domain::DirectoryPath, domain::FileName>>, std::string>
    list_regular_files_recursive(const domain::DirectoryPath&) override {
        return {};
    }
    std::expected<std::vector<domain::DirectoryPath>, std::string> list_directories_recursive(
        const domain::DirectoryPath& directory) override {
        if (directory_error) return std::unexpected("directory error");
        if (!directories.empty()) return directories;
        return std::vector<domain::DirectoryPath>{directory};
    }
};

class Manifests final : public ports::ManifestRepository {
   public:
    bool load_error = false;
    bool save_error = false;
    std::optional<domain::DirectoryManifest> value;
    std::expected<std::optional<domain::DirectoryManifest>, std::string> load(
        const domain::DirectoryPath&) override {
        if (load_error) return std::unexpected("manifest error");
        return value;
    }
    std::expected<void, std::string> save(const domain::DirectoryManifest&) override {
        if (save_error) return std::unexpected("save error");
        return {};
    }
};

class Index final : public ports::SearchIndexRepository {
   public:
    bool save_error = false;
    bool load_error = false;
    std::expected<void, std::string> save(const domain::DirectoryPath&,
                                          const domain::SearchIndex&) override {
        if (save_error) return std::unexpected("index save error");
        return {};
    }
    std::expected<domain::SearchIndex, std::string> load(
        const domain::DirectoryPath&) const override {
        if (load_error) return std::unexpected("index load error");
        return domain::SearchIndex{};
    }
    std::expected<std::uintmax_t, std::string> size(const domain::DirectoryPath&) const override {
        return 0;
    }
};

void application_error_tests() {
    const auto directory = domain::DirectoryPath::create(".");
    Filesystem filesystem;
    Manifests manifests;
    Index index;
    application::RebuildSearchIndex rebuild(manifests, filesystem, index);
    filesystem.directory_error = true;
    const auto directory_error = rebuild.execute(directory);
    assert(!directory_error);
    filesystem.directory_error = false;
    manifests.load_error = true;
    const auto load_error = rebuild.execute(directory);
    assert(!load_error);
    manifests.load_error = false;
    index.save_error = true;
    const auto save_error = rebuild.execute(directory);
    assert(!save_error);

    application::SearchManifest search(manifests, filesystem);
    filesystem.directory_error = true;
    assert(!search.execute(directory, "valid"));
    filesystem.directory_error = false;
    manifests.load_error = true;
    assert(!search.execute(directory, "valid"));
    manifests.load_error = false;
    Index missing_index;
    missing_index.load_error = true;
    application::SearchManifest indexed_search(manifests, filesystem, &missing_index);
    assert(indexed_search.execute(directory, "valid").has_value());

    auto sortable = domain::DirectoryManifest::create(directory_id, directory);
    domain::FileMetadata first_metadata;
    first_metadata.type = "text/plain";
    first_metadata.created_ns = 1;
    first_metadata.extension_fields["search"] = "common";
    sortable.add({file_id, domain::FileName::create("alpha.txt"), first_metadata,
                  domain::Fingerprint::create("test:alpha")});
    auto second_metadata = first_metadata;
    second_metadata.modified_ns = 2;
    sortable.add({domain::FileId::create("01K1AC4K3N7JZM5F21V6PH8QRT"),
                  domain::FileName::create("beta.txt"), second_metadata,
                  domain::Fingerprint::create("test:beta")});
    sortable.add({domain::FileId::create("01K1AEQ1ZC5R7H4XB8QKMP2TV6"),
                  domain::FileName::create("gamma.txt"), second_metadata,
                  domain::Fingerprint::create("test:gamma")});
    manifests.value = sortable;
    filesystem.directories = {directory};
    auto sorted = search.execute(directory, "common");
    assert(sorted.has_value() && sorted->matches.size() == 3);
    filesystem.directories = {directory, domain::DirectoryPath::create("other")};
    auto directory_sorted = search.execute(directory, "common");
    assert(directory_sorted.has_value() && directory_sorted->matches.size() >= 2);
}
}  // namespace

int main() {
    try {
        value_and_manifest_tests();
        identity_tests();
        application_error_tests();
    } catch (...) {
        return 1;
    }
    return 0;
}
