#include "manifests/infrastructure/toml_manifest_repository.hpp"

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <thread>
#include <cstddef>
#include <exception>
#include <iterator>
#include <system_error>
#include <utility>
#include <vector>

namespace crumb::infrastructure {
namespace testing {
std::expected<std::optional<domain::DirectoryManifest>, std::string> default_load_function(
    TomlManifestRepository& repository, const domain::DirectoryPath& directory) {
    return repository.load(directory);
}

LoadFunction load_function = default_load_function;
}  // namespace testing

std::expected<std::optional<domain::DirectoryManifest>, std::string> TomlManifestRepository::load(
    const domain::DirectoryPath& directory) {
    const auto path = std::filesystem::path(directory.value()) / ".crumb";
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        std::error_code error;
        if (!std::filesystem::exists(path, error)) return std::nullopt;
        return std::unexpected("cannot read " + path.string());
    }
    const std::string text((std::istreambuf_iterator<char>(input)),
                           std::istreambuf_iterator<char>());
    auto manifest = mapper_.fromToml(text, directory);
    if (!manifest) return std::unexpected(manifest.error());
    return std::optional<domain::DirectoryManifest>(std::move(manifest.value()));
}
std::expected<ports::LoadedManifestBatch, std::string> TomlManifestRepository::load_many(
    const std::vector<domain::DirectoryPath>& directories) {
    using LoadResult = std::expected<std::optional<domain::DirectoryManifest>, std::string>;
    std::vector<std::optional<LoadResult>> loaded(directories.size());
    std::atomic<std::size_t> next{};
    const auto hardware_threads = std::thread::hardware_concurrency();
    const auto worker_count =
        std::min<std::size_t>(directories.size(), std::max<std::size_t>(1, hardware_threads));
    std::vector<std::thread> workers;
    workers.reserve(worker_count);
    for (std::size_t worker = 0; worker < worker_count; ++worker) {
        workers.emplace_back([&] {
            while (true) {
                const auto index = next.fetch_add(1, std::memory_order_relaxed);
                if (index >= directories.size()) return;
                try {
                    loaded[index] = testing::load_function(*this, directories[index]);
                } catch (const std::exception& error) {
                    loaded[index] = std::unexpected(error.what());
                }
            }
        });
    }
    for (auto& worker : workers) worker.join();

    ports::LoadedManifestBatch result;
    result.reserve(directories.size());
    for (std::size_t index = 0; index < directories.size(); ++index) {
        if (!loaded[index]) return std::unexpected("manifest worker did not return a result");
        auto& manifest = *loaded[index];
        if (!manifest) return std::unexpected(manifest.error());
        result.emplace_back(directories[index], std::move(*manifest));
    }
    return result;
}
std::expected<void, std::string> TomlManifestRepository::save(
    const domain::DirectoryManifest& manifest) {
    const auto directory = std::filesystem::path(manifest.path().value());
    const auto temporary = directory / ".crumb.tmp";
    const auto target = directory / ".crumb";
    try {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) return std::unexpected("cannot write " + temporary.string());
        output << mapper_.toToml(manifest);
        output.flush();
        if (!output) return std::unexpected("cannot flush " + temporary.string());
        output.close();
        std::filesystem::rename(temporary, target);
    } catch (const std::exception& error) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        return std::unexpected(error.what());
    }
    return {};
}
}  // namespace crumb::infrastructure
