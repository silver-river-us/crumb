#include "application/reconcile_directory.hpp"
#include "infrastructure/filesystem/native_filesystem.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>

namespace {
class InMemoryManifestRepository final : public crumb::ports::ManifestRepository {
public:
    std::optional<crumb::domain::DirectoryManifest> manifest;

    std::expected<std::optional<crumb::domain::DirectoryManifest>, std::string>
    load(const crumb::domain::DirectoryPath&) override {
        return manifest;
    }

    std::expected<void, std::string> save(const crumb::domain::DirectoryManifest& value) override {
        manifest = value;
        return {};
    }
};

class NoopFingerprintService final : public crumb::ports::FingerprintService {
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

class TestIds final : public crumb::ports::IdGenerator {
public:
    crumb::domain::DirectoryId directory_id() override {
        return crumb::domain::DirectoryId::create("01K1AB5YZ4QH7M2D8E3F9G6JNX");
    }

    crumb::domain::FileId file_id() override {
        return crumb::domain::FileId::create("01K1ADN1ZC5R7H4XB8QKMP2TV6");
    }
};

class TestClock final : public crumb::ports::Clock {
public:
    std::string now_utc() override { return "2026-08-11T00:00:00Z"; }
};
}

int main() {
    const auto directory = std::filesystem::temp_directory_path() / "crumb-reconcile-metadata-test";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    {
        std::ofstream file(directory / "notes.txt");
        file << "initial content";
    }

    InMemoryManifestRepository manifests;
    NoopFingerprintService fingerprints;
    TestIds ids;
    TestClock clock;
    crumb::infrastructure::NativeFileSystem filesystem;
    crumb::application::ReconcileDirectory reconcile(
        manifests, filesystem, fingerprints, filesystem, ids, clock);
    const auto path = crumb::domain::DirectoryPath::create(directory.string());

    assert(reconcile.execute(path).has_value());
    assert(manifests.manifest.has_value());
    manifests.manifest->files()[0].metadata.extension_fields["custom.owner"] = "legal";

    {
        std::ofstream file(directory / "notes.txt", std::ios::trunc);
        file << "changed content with a new fingerprint";
    }
    assert(reconcile.execute(path).has_value());
    assert(manifests.manifest->files()[0].metadata.extension_fields.at("custom.owner") == "legal");

    std::filesystem::remove_all(directory);
}
