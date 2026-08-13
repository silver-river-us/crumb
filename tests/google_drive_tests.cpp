#include "plugins/google_drive/google_drive_plugin.hpp"
#include "domain/value_objects/content_hash.hpp"
#include "domain/value_objects/directory_id.hpp"
#include "domain/value_objects/file_id.hpp"
#include "domain/value_objects/fingerprint.hpp"
#include "lib/ports/clock.hpp"
#include "lib/ports/fingerprint_service.hpp"
#include "lib/ports/id_generator.hpp"
#include "lib/ports/manifest_repository.hpp"
#include "lib/ports/search_index_repository.hpp"

#include <cassert>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <string>
#include <vector>

#ifdef __APPLE__
#include <sys/xattr.h>
#endif

namespace {
using namespace crumb;

const auto directory_id_constant = domain::DirectoryId::create("01K1AB5YZ4QH7M2D8E3F9G6JNX");
const auto file_id_constant = domain::FileId::create("01K1ADN1ZC5R7H4XB8QKMP2TV6");

#ifdef __APPLE__
int fail_pipe(int*) { return -1; }
pid_t fail_fork() { return -1; }
#endif

std::filesystem::path temp_root() {
    auto path = std::filesystem::temp_directory_path() /
                ("crumb-drive-" +
                 std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::remove_all(path);
    std::filesystem::create_directories(path);
    return path;
}

domain::DirectoryManifest manifest_at(const std::filesystem::path& path) {
    auto manifest = domain::DirectoryManifest::create(
        directory_id_constant, domain::DirectoryPath::create(path.string()), "now", "test");
    domain::FileMetadata metadata;
    metadata.type = "text/plain";
    metadata.size = 4;
    metadata.modified_ns = 2;
    manifest.add({file_id_constant, domain::FileName::create("note.txt"), metadata,
                  domain::Fingerprint::create("drive-stat:4:2:1:1")});
    return manifest;
}

class ManifestRepository final : public ports::ManifestRepository {
   public:
    std::map<std::string, std::optional<domain::DirectoryManifest>> values;
    bool fail_load = false;
    bool fail_save = false;

    std::expected<std::optional<domain::DirectoryManifest>, std::string> load(
        const domain::DirectoryPath& path) override {
        if (fail_load) return std::unexpected("manifest load failed");
        const auto found = values.find(path.value());
        return found == values.end() ? std::optional<domain::DirectoryManifest>{} : found->second;
    }
    std::expected<void, std::string> save(const domain::DirectoryManifest& manifest) override {
        if (fail_save) return std::unexpected("manifest save failed");
        values[manifest.path().value()] = manifest;
        return {};
    }
};

class SearchIndexRepository final : public ports::SearchIndexRepository {
   public:
    std::optional<domain::SearchIndex> value;
    bool fail_save = false;
    bool fail_load = false;
    bool fail_size = false;

    std::expected<void, std::string> save(const domain::DirectoryPath&,
                                          const domain::SearchIndex& index) override {
        if (fail_save) return std::unexpected("index save failed");
        value = index;
        return {};
    }
    std::expected<domain::SearchIndex, std::string> load(
        const domain::DirectoryPath&) const override {
        if (fail_load || !value) return std::unexpected("index load failed");
        return *value;
    }
    std::expected<std::uintmax_t, std::string> size(const domain::DirectoryPath&) const override {
        if (fail_size) return std::unexpected("index size failed");
        return value ? 42 : 0;
    }
};

class Fingerprints final : public ports::FingerprintService {
   public:
    std::expected<domain::Fingerprint, std::string> fingerprint(const domain::DirectoryPath&,
                                                                const domain::FileName&) override {
        return domain::Fingerprint::create("drive:fingerprint");
    }
    std::expected<domain::ContentHash, std::string> content_hash(const domain::DirectoryPath&,
                                                                 const domain::FileName&) override {
        return domain::ContentHash::create("drive:content");
    }
};

class Ids final : public ports::IdGenerator {
   public:
    domain::DirectoryId directory_id() override { return directory_id_value; }
    domain::FileId file_id() override { return file_id_value; }
    domain::DirectoryId directory_id_value = directory_id_constant;
    domain::FileId file_id_value = file_id_constant;
};

class Clock final : public ports::Clock {
   public:
    std::string now_utc() override { return "2026-08-12T00:00:00Z"; }
};

void repository_tests(const std::filesystem::path& root) {
    ManifestRepository delegate;
    plugins::google_drive::DriveManifestRepository manifests(delegate);
    auto source = domain::DirectoryPath::create(root.string());
    const auto initial_load = manifests.load(source);
    assert(!initial_load);
    manifests.set_source_root(source);
    const auto cached_load = manifests.load(source);
    assert(cached_load.has_value());
    delegate.values[(root / "cache").string()] = std::nullopt;
    const auto manifest_save = manifests.save(manifest_at(root));
    assert(manifest_save);
    assert(!delegate.values.empty());
    auto loaded = manifests.load(source);
    assert(loaded.has_value() && loaded->has_value());
    assert(loaded->value().path().value() == root.string());
    delegate.fail_load = true;
    const auto failed_load = manifests.load(source);
    assert(!failed_load);
    delegate.fail_load = false;
    delegate.fail_save = true;
    const auto failed_save = manifests.save(manifest_at(root));
    assert(!failed_save);

    SearchIndexRepository index_delegate;
    plugins::google_drive::DriveSearchIndexRepository indexes(index_delegate);
    assert(!indexes.load(source));
    assert(!indexes.size(source));
    domain::SearchIndex index;
    indexes.set_cache_root(root / "index-cache");
    const auto index_save = indexes.save(source, index);
    assert(index_save);
    const auto index_load = indexes.load(source);
    assert(index_load.has_value());
    assert(indexes.size(source).value() == 42);
    index_delegate.fail_save = true;
    const auto failed_index_save = indexes.save(source, index);
    assert(!failed_index_save);
    index_delegate.fail_save = false;
    index_delegate.fail_load = true;
    assert(!indexes.load(source));
    index_delegate.fail_load = false;
    index_delegate.fail_size = true;
    assert(!indexes.size(source));

    const auto bad_cache_parent = root / "cache-file";
    std::ofstream(bad_cache_parent) << "not a directory";
    setenv("XDG_CACHE_HOME", bad_cache_parent.c_str(), 1);
    manifests.set_source_root(source);
    const auto bad_manifest_save = manifests.save(manifest_at(root));
    assert(!bad_manifest_save);
    indexes.set_cache_root(bad_cache_parent / "crumb");
    const auto bad_index_save = indexes.save(source, index);
    assert(!bad_index_save);
    setenv("XDG_CACHE_HOME", (root / "cache").c_str(), 1);
    unsetenv("HOME");
    manifests.set_source_root(source);
    setenv("HOME", (root / "home").c_str(), 1);
    manifests.set_source_root(source);
}

void filesystem_tests(const std::filesystem::path& root) {
    std::filesystem::create_directories(root);
    std::ofstream(root / "z.pdf") << "pdf";
    std::ofstream(root / "a.md") << "Drive text and drive text";
    std::ofstream(root / "b.docx") << "office";
    std::ofstream(root / "c.rtf") << "office";
    std::ofstream(root / "d.html") << "html";
    std::ofstream(root / "e.htm") << "html";
    std::ofstream(root / "f.unknown") << "unknown";
    std::ofstream(root / "quote'name.txt") << "quoted path";
    std::ofstream(root / ".crumb") << "ignored";
    std::filesystem::create_directories(root / ".Trash");
    std::ofstream(root / ".Trash" / "ignored.txt") << "ignored";
    std::filesystem::create_directories(root / "nested");
    std::ofstream(root / "nested" / "note.doc") << "office";
    infrastructure::NativeFileSystem native;
    plugins::google_drive::DriveFileSystem filesystem(native);
    const auto path = domain::DirectoryPath::create(root.string());
    auto files = filesystem.list_regular_files(path);
    assert(files.has_value() && files->size() == 8);
    assert(files->front().name.value() == "a.md");
    assert(files->front().metadata.type == "text/markdown");
    auto recursive = filesystem.list_regular_files_recursive(path);
    assert(recursive.has_value() && recursive->size() == 9);
    auto directories = filesystem.list_directories_recursive(path);
    assert(directories.has_value() && directories->size() == 2);
    const auto read_text = filesystem.read_text_file(path, domain::FileName::create("a.md"));
    assert(read_text->has_value());
    const auto missing_files =
        filesystem.list_regular_files(domain::DirectoryPath::create((root / "missing").string()));
    assert(!missing_files);
    const auto missing_recursive = filesystem.list_regular_files_recursive(
        domain::DirectoryPath::create((root / "missing").string()));
    assert(!missing_recursive);
    const auto missing_directories = filesystem.list_directories_recursive(
        domain::DirectoryPath::create((root / "missing").string()));
    assert(!missing_directories);

#ifdef __APPLE__
    constexpr char item_attribute[] = "com.google.drivefs.item-id#S";
    const char valid_item_id[] = "drive_item-123";
    assert(::setxattr((root / "a.md").c_str(), item_attribute, valid_item_id,
                      sizeof valid_item_id - 1, 0, 0) == 0);
    auto with_url = filesystem.list_regular_files(path);
    assert(with_url->front().metadata.external_url ==
           plugins::google_drive::GoogleDrivePlugin::url_for_item_id("drive_item-123"));
    const char invalid_item_id[] = "invalid/item";
    assert(::setxattr((root / "z.pdf").c_str(), item_attribute, invalid_item_id,
                      sizeof invalid_item_id - 1, 0, 0) == 0);
#endif

    plugins::google_drive::DriveMetadataExtractor extractor;
    unsetenv("CRUMB_DRIVE_CONTENT");
    auto metadata = extractor.extract(path, domain::FileName::create("a.md"), {});
    assert(metadata.has_value());
    assert(metadata->title == "a");
#ifdef __APPLE__
    assert(metadata->extension_fields.at("crumb.search_terms_v2").find("drive") !=
           std::string::npos);
#else
    assert(!metadata->extension_fields.contains("crumb.search_terms_v2"));
#endif
    auto office = extractor.extract(domain::DirectoryPath::create((root / "nested").string()),
                                    domain::FileName::create("note.doc"), {});
    assert(office.has_value());
    setenv("CRUMB_DRIVE_CONTENT", "1", 1);
    auto office_enabled =
        extractor.extract(domain::DirectoryPath::create((root / "nested").string()),
                          domain::FileName::create("note.doc"), {});
    assert(office_enabled.has_value());
    const auto a_metadata = extractor.extract(path, domain::FileName::create("a.md"), {});
    const auto pdf_metadata = extractor.extract(path, domain::FileName::create("z.pdf"), {});
    const auto docx_metadata = extractor.extract(path, domain::FileName::create("b.docx"), {});
    const auto rtf_metadata = extractor.extract(path, domain::FileName::create("c.rtf"), {});
    const auto html_metadata = extractor.extract(path, domain::FileName::create("d.html"), {});
    const auto htm_metadata = extractor.extract(path, domain::FileName::create("e.htm"), {});
    const auto unknown_metadata =
        extractor.extract(path, domain::FileName::create("f.unknown"), {});
    const auto quoted_metadata =
        extractor.extract(path, domain::FileName::create("quote'name.txt"), {});
    assert(a_metadata.has_value());
    assert(pdf_metadata.has_value());
    assert(docx_metadata.has_value());
    assert(rtf_metadata.has_value());
    assert(html_metadata.has_value());
    assert(htm_metadata.has_value());
    assert(unknown_metadata.has_value());
    assert(quoted_metadata.has_value());
#ifdef __APPLE__
    const auto old_pipe = plugins::google_drive::testing::pipe_function;
    const auto old_fork = plugins::google_drive::testing::fork_process;
    plugins::google_drive::testing::pipe_function = fail_pipe;
    assert(extractor.extract(path, domain::FileName::create("a.md"), {}).has_value());
    plugins::google_drive::testing::pipe_function = old_pipe;
    plugins::google_drive::testing::fork_process = fail_fork;
    assert(extractor.extract(path, domain::FileName::create("a.md"), {}).has_value());
    plugins::google_drive::testing::fork_process = old_fork;
#endif
    {
        std::ofstream output(root / "large.txt", std::ios::binary);
        output.seekp(8ULL * 1024 * 1024);
        output.put('x');
    }
    const auto large_metadata = extractor.extract(path, domain::FileName::create("large.txt"), {});
    assert(large_metadata.has_value());
    unsetenv("CRUMB_DRIVE_CONTENT");
}

void plugin_tests(const std::filesystem::path& root) {
    const auto source = root / "source";
    std::filesystem::create_directories(source);
    std::ofstream(source / "proposal.md") << "Technical proposal";
    std::filesystem::create_directories(source / ".tmp");
    std::ofstream(source / ".tmp" / "ignored.md") << "ignored";
    const auto cache = root / "cache";
    setenv("XDG_CACHE_HOME", cache.c_str(), 1);
    setenv("HOME", (root / "home").c_str(), 1);
    ManifestRepository manifests;
    SearchIndexRepository indexes;
    Fingerprints fingerprints;
    Ids ids;
    Clock clock;
    infrastructure::NativeFileSystem native;
    plugins::google_drive::GoogleDrivePlugin plugin(native, manifests, indexes, fingerprints, ids,
                                                    clock);
    assert(plugin.resolve(source.string()).value().value() == source.string());
    assert(!plugin.resolve((root / "missing").string()));
    auto indexed = plugin.index(source.string());
    assert(indexed.has_value());
    assert(indexed->reconcile.scanned == 1);
    assert(indexes.value.has_value());
    auto searched = plugin.search(domain::DirectoryPath::create(source.string()), "technical", 10);
    assert(searched.has_value());
#ifdef __APPLE__
    assert(searched->matches.size() == 1);
#else
    assert(searched->matches.empty());
#endif
    const auto missing_index = plugin.index((root / "missing").string());
    assert(!missing_index);

    const auto old_home = std::getenv("HOME");
    (void)old_home;
    setenv("HOME", (root / "no-mount-home").c_str(), 1);
    assert(!plugin.resolve());
    std::filesystem::create_directories(root / "empty-home" / "Library" / "CloudStorage");
    setenv("HOME", (root / "empty-home").c_str(), 1);
    assert(!plugin.resolve());
    std::filesystem::create_directories(root / "one-home" / "Library" / "CloudStorage" /
                                        "GoogleDrive-one");
    setenv("HOME", (root / "one-home").c_str(), 1);
    assert(plugin.resolve().has_value());
    std::filesystem::create_directories(root / "one-home" / "Library" / "CloudStorage" /
                                        "GoogleDrive-two");
    assert(!plugin.resolve());
    unsetenv("HOME");
    assert(!plugin.resolve());
    unsetenv("XDG_CACHE_HOME");
    plugins::google_drive::DriveManifestRepository cache_probe(manifests);
    cache_probe.set_source_root(domain::DirectoryPath::create(source.string()));
}
}  // namespace

int main() {
    try {
        assert(crumb::plugins::google_drive::GoogleDrivePlugin::url_for_item_id("abc_123-xyz") ==
               "https://drive.google.com/open?id=abc_123-xyz");
        const auto root = temp_root();
        repository_tests(root);
        filesystem_tests(root / "filesystem");
        plugin_tests(root);
        std::filesystem::remove_all(root);
    } catch (...) {
        return 1;
    }
    return 0;
}
