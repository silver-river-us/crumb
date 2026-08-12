#include "boundary/cli/command_router.hpp"
#include "infrastructure/composition/application_builder.hpp"

#include <cassert>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace {
FILE* fail_popen(const char*, const char*) { return nullptr; }

struct Result {
    int status;
    std::string output;
    std::string error;
};

Result run(crumb::boundary::CommandRouter& router, std::initializer_list<std::string> arguments) {
    std::vector<std::string> values(arguments);
    std::vector<char*> argv;
    for (auto& value : values) argv.push_back(value.data());
    std::ostringstream output;
    std::ostringstream error;
    auto* old_output = std::cout.rdbuf(output.rdbuf());
    auto* old_error = std::cerr.rdbuf(error.rdbuf());
    const auto status = router.run(static_cast<int>(argv.size()), argv.data());
    std::cout.rdbuf(old_output);
    std::cerr.rdbuf(old_error);
    return {status, output.str(), error.str()};
}

void run_interactive_table(crumb::boundary::CommandRouter& router,
                           const std::filesystem::path& root) {
    const auto less = root / "less";
    {
        std::ofstream script(less);
        script << "#!/bin/sh\ncat >/dev/null\n";
    }
    assert(::chmod(less.c_str(), 0755) == 0);
    const auto old_path = std::getenv("PATH");
    const auto path = root.string() + ":" + (old_path == nullptr ? "" : old_path);
    setenv("PATH", path.c_str(), 1);

    const int master = ::posix_openpt(O_RDWR | O_NOCTTY);
    assert(master >= 0 && ::grantpt(master) == 0 && ::unlockpt(master) == 0);
    const int slave = ::open(::ptsname(master), O_RDWR | O_NOCTTY);
    assert(slave >= 0);
    const int saved_stdout = ::dup(STDOUT_FILENO);
    assert(saved_stdout >= 0 && ::dup2(slave, STDOUT_FILENO) >= 0);
    const auto result = run(router, {"crumb", "search", "technical", "--table"});
    assert(result.status == 0);
    const auto old_popen = crumb::boundary::testing::popen_function;
    crumb::boundary::testing::popen_function = fail_popen;
    const auto fallback = run(router, {"crumb", "search", "technical", "--table"});
    assert(fallback.status == 0);
    crumb::boundary::testing::popen_function = old_popen;
    assert(::dup2(saved_stdout, STDOUT_FILENO) >= 0);
    ::close(saved_stdout);
    ::close(slave);
    ::close(master);
    if (old_path == nullptr)
        unsetenv("PATH");
    else
        setenv("PATH", old_path, 1);
}

class FakeManifestRepository final : public crumb::ports::ManifestRepository {
   public:
    std::optional<crumb::domain::DirectoryManifest> value;
    std::expected<std::optional<crumb::domain::DirectoryManifest>, std::string> load(
        const crumb::domain::DirectoryPath&) override {
        return value;
    }
    std::expected<void, std::string> save(
        const crumb::domain::DirectoryManifest& manifest) override {
        value = manifest;
        return {};
    }
};

class FakeFileSystem final : public crumb::ports::FileSystem {
   public:
    std::vector<crumb::domain::DirectoryPath> directories;
    std::expected<std::vector<crumb::domain::FileSnapshot>, std::string> list_regular_files(
        const crumb::domain::DirectoryPath&) override {
        return std::vector<crumb::domain::FileSnapshot>{};
    }
    std::expected<std::optional<std::string>, std::string> read_text_file(
        const crumb::domain::DirectoryPath&, const crumb::domain::FileName&) override {
        return std::nullopt;
    }
    std::expected<std::vector<std::pair<crumb::domain::DirectoryPath, crumb::domain::FileName>>,
                  std::string>
    list_regular_files_recursive(const crumb::domain::DirectoryPath&) override {
        return {};
    }
    std::expected<std::vector<crumb::domain::DirectoryPath>, std::string>
    list_directories_recursive(const crumb::domain::DirectoryPath& directory) override {
        return directories.empty() ? std::vector<crumb::domain::DirectoryPath>{directory}
                                   : directories;
    }
};

class FakeFingerprint final : public crumb::ports::FingerprintService {
   public:
    std::expected<crumb::domain::Fingerprint, std::string> fingerprint(
        const crumb::domain::DirectoryPath&, const crumb::domain::FileName&) override {
        return crumb::domain::Fingerprint::create("test:fingerprint");
    }
    std::expected<crumb::domain::ContentHash, std::string> content_hash(
        const crumb::domain::DirectoryPath&, const crumb::domain::FileName&) override {
        return crumb::domain::ContentHash::create("test:content");
    }
};

class FakeIds final : public crumb::ports::IdGenerator {
   public:
    crumb::domain::DirectoryId directory_id() override {
        return crumb::domain::DirectoryId::create("01K1AB5YZ4QH7M2D8E3F9G6JNX");
    }
    crumb::domain::FileId file_id() override {
        return crumb::domain::FileId::create("01K1ADN1ZC5R7H4XB8QKMP2TV6");
    }
};

class FakeExtractor final : public crumb::ports::MetadataExtractor {
   public:
    std::expected<crumb::domain::FileMetadata, std::string> extract(
        const crumb::domain::DirectoryPath&, const crumb::domain::FileName&,
        crumb::domain::FileMetadata metadata) override {
        return metadata;
    }
};

class FakeClock final : public crumb::ports::Clock {
   public:
    std::string now_utc() override { return "now"; }
};

class FailingIndex final : public crumb::ports::SearchIndexRepository {
   public:
    std::expected<void, std::string> save(const crumb::domain::DirectoryPath&,
                                          const crumb::domain::SearchIndex&) override {
        return std::unexpected("save failed");
    }
    std::expected<crumb::domain::SearchIndex, std::string> load(
        const crumb::domain::DirectoryPath&) const override {
        return std::unexpected("load failed");
    }
    std::expected<std::uintmax_t, std::string> size(
        const crumb::domain::DirectoryPath&) const override {
        return 0;
    }
};

std::filesystem::path temp_root() {
    auto path = std::filesystem::temp_directory_path() /
                ("crumb-router-" +
                 std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::remove_all(path);
    std::filesystem::create_directories(path);
    return path;
}
}  // namespace

int main() {
    const auto root = temp_root();
    std::ofstream(root / "technical proposal.md") << "technical proposal";
    std::ofstream(root / "this-is-a-very-long-file-name-that-needs-to-be-shortened-for-display.txt")
        << "long technical proposal";
    std::filesystem::create_directories(root / "nested");
    std::ofstream(root / "nested" / "annual.txt") << "annual technical proposal";
    const auto old_current = std::filesystem::current_path();
    std::filesystem::current_path(root);

    crumb::infrastructure::ApplicationComponents components;
    auto config = crumb::boundary::UserConfig::load(root / "missing.conf", root);
    assert(config.has_value());
    crumb::boundary::CommandRouter router(components.reconcile, components.rebuild_index,
                                          components.search, components.index_size,
                                          components.drive, *config);

    auto unknown = run(router, {"crumb", "unknown"});
    assert(unknown.status == 2 && unknown.error.find("usage:") != std::string::npos);
    const auto invalid_match = crumb::application::SearchMatch{
        crumb::domain::DirectoryPath::create(std::string("bad\0directory", 13)),
        crumb::domain::FileName::create("file.txt")};
    assert(!crumb::boundary::testing::relative_result_path_for_test(
                invalid_match, crumb::domain::DirectoryPath::create("."))
                .empty());
    auto index_size_usage = run(router, {"crumb", "index_size", ".", "extra"});
    assert(index_size_usage.status == 2);
    auto missing_index = run(router, {"crumb", "index_size", "."});
    assert(missing_index.status == 1 && missing_index.error.find("crumb:") != std::string::npos);
    auto bad_scan = run(router, {"crumb", "scan", (root / "missing").string()});
    assert(bad_scan.status == 1);
    auto no_search_args = run(router, {"crumb", "search"});
    assert(no_search_args.status == 2);
    auto bad_tap = run(router, {"crumb", "search", "technical", "--tap=xml"});
    assert(bad_tap.status == 2);
    auto missing_limit = run(router, {"crumb", "search", "technical", "--limit"});
    assert(missing_limit.status == 2);
    auto bad_limit = run(router, {"crumb", "search", "technical", "--limit=nope"});
    assert(bad_limit.status == 2);
    auto empty_limit = run(router, {"crumb", "search", "technical", "--limit="});
    assert(empty_limit.status == 2);
    auto too_many = run(router, {"crumb", "search", "one", "two", "three"});
    assert(too_many.status == 2);
    auto bad_query = run(router, {"crumb", "search", "a"});
    assert(bad_query.status == 1);

    auto scan = run(router, {"crumb", "scan", "."});
    assert(scan.status == 0 && scan.output.find("scanned=") != std::string::npos);
    auto size = run(router, {"crumb", "index_size"});
    assert(size.status == 0 && size.output.find("index_size_bytes=") != std::string::npos);
    auto loaded_manifest =
        components.manifests.load(crumb::domain::DirectoryPath::create(root.string()));
    assert(loaded_manifest.has_value() && loaded_manifest->has_value());
    for (auto& file : loaded_manifest->value().files()) {
        file.metadata.external_url = "https://example.test/item";
        file.metadata.author = "Author";
    }
    assert(components.manifests.save(loaded_manifest->value()));
    std::filesystem::remove(root / ".crumb.index");
    auto full = run(
        router, {"crumb", "search", ".", "technical", "--limit", "1", "--details", "--tap=text"});
    assert(full.status == 0 && full.output.find("Search results:") != std::string::npos &&
           full.output.find("tap query=") != std::string::npos);
    auto table = run(router, {"crumb", "search", "technical", "--table", "--tap"});
    assert(table.status == 0 && table.output.find("| Name") != std::string::npos);
    auto html = run(router, {"crumb", "search", "technical", "--tap", "html"});
    assert(html.status == 0 && html.output.find("<!doctype html>") != std::string::npos);
    auto tap_equals_html = run(router, {"crumb", "search", "technical", "--tap=html"});
    assert(tap_equals_html.status == 0 &&
           tap_equals_html.output.find("<!doctype html>") != std::string::npos);
    run_interactive_table(router, root);

    setenv("XDG_CACHE_HOME", (root / "cache").c_str(), 1);
    setenv("HOME", (root / "home").c_str(), 1);
    std::filesystem::create_directories(root / "home" / "Library" / "CloudStorage" /
                                        "GoogleDrive-test");
    auto bad_index = run(router, {"crumb", "index", "wrong"});
    assert(bad_index.status == 2);
    auto missing_drive = run(router, {"crumb", "index", "drive", (root / "missing").string()});
    assert(missing_drive.status == 1);
    auto drive_index = run(router, {"crumb", "index", "drive", root.string()});
    assert(drive_index.status == 0 && drive_index.output.find("source=drive") != std::string::npos);
    auto drive_search = run(router, {"crumb", "search", "drive", "technical", "--limit=1"});
    assert(drive_search.status == 0);
    unsetenv("HOME");
    auto drive_no_mount = run(router, {"crumb", "search", "drive", "technical"});
    assert(drive_no_mount.status == 1);

    FakeManifestRepository fake_manifests;
    FakeFileSystem fake_filesystem;
    FakeFingerprint fake_fingerprints;
    FakeExtractor fake_extractor;
    FakeIds fake_ids;
    FakeClock fake_clock;
    FailingIndex failing_index;
    crumb::infrastructure::NativeFileSystem fake_native;
    crumb::plugins::google_drive::GoogleDrivePlugin fake_drive(
        fake_native, fake_manifests, failing_index, fake_fingerprints, fake_ids, fake_clock);
    crumb::application::ReconcileDirectory fake_reconcile(
        fake_manifests, fake_filesystem, fake_fingerprints, fake_extractor, fake_ids, fake_clock);
    crumb::application::RebuildSearchIndex fake_rebuild(fake_manifests, fake_filesystem,
                                                        failing_index);
    crumb::application::SearchManifest fake_search(fake_manifests, fake_filesystem, &failing_index);
    crumb::application::IndexSize fake_size(failing_index);
    crumb::boundary::CommandRouter failing_router(fake_reconcile, fake_rebuild, fake_search,
                                                  fake_size, fake_drive, *config);
    auto rebuild_failure = run(failing_router, {"crumb", "scan", root.string()});
    assert(rebuild_failure.status == 1 &&
           rebuild_failure.error.find("save failed") != std::string::npos);

    auto outside_manifest = crumb::domain::DirectoryManifest::create(
        crumb::domain::DirectoryId::create("01K1AB5YZ4QH7M2D8E3F9G6JNX"),
        crumb::domain::DirectoryPath::create("/tmp/outside"));
    crumb::domain::FileMetadata outside_metadata;
    outside_metadata.type = "text/plain";
    outside_metadata.external_url = "https://example.test/outside";
    outside_metadata.extension_fields["search"] = "outside";
    outside_manifest.add({crumb::domain::FileId::create("01K1ADN1ZC5R7H4XB8QKMP2TV6"),
                          crumb::domain::FileName::create("outside.txt"), outside_metadata,
                          crumb::domain::Fingerprint::create("test:outside")});
    fake_manifests.value = outside_manifest;
    fake_filesystem.directories = {crumb::domain::DirectoryPath::create("/tmp/outside")};
    auto outside = run(failing_router, {"crumb", "search", root.string(), "outside", "--table"});
    assert(outside.status == 0 && outside.output.find("outside.txt") != std::string::npos);

    const auto invalid_directory =
        crumb::domain::DirectoryPath::create(std::string("bad\0root", 8));
    auto invalid_manifest = crumb::domain::DirectoryManifest::create(
        crumb::domain::DirectoryId::create("01K1AB5YZ4QH7M2D8E3F9G6JNX"), invalid_directory);
    invalid_manifest.add({crumb::domain::FileId::create("01K1ADN1ZC5R7H4XB8QKMP2TV6"),
                          crumb::domain::FileName::create("outside.txt"), outside_metadata,
                          crumb::domain::Fingerprint::create("test:outside")});
    fake_manifests.value = invalid_manifest;
    fake_filesystem.directories = {invalid_directory};
    auto invalid_path =
        run(failing_router, {"crumb", "search", root.string(), "outside", "--table"});
    assert(invalid_path.status == 0);

    const auto executable = std::filesystem::absolute("build/coverage/crumb");
    assert(std::system(
               ("env -u HOME " + executable.string() + " scan . >/dev/null 2>/dev/null").c_str()) !=
           0);

    std::filesystem::current_path(old_current);
    std::filesystem::remove_all(root);
}
