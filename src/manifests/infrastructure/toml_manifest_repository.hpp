#pragma once
#include "manifests/infrastructure/toml_manifest_mapper.hpp"
#include "manifests/application/manifest_repository.hpp"

namespace crumb::infrastructure {
class TomlManifestRepository;
namespace testing {
using LoadFunction = std::expected<std::optional<domain::DirectoryManifest>, std::string> (*)(
    TomlManifestRepository&, const domain::DirectoryPath&);
extern LoadFunction load_function;
}  // namespace testing

class TomlManifestRepository final : public ports::ManifestRepository {
   public:
    std::expected<std::optional<domain::DirectoryManifest>, std::string> load(
        const domain::DirectoryPath&) override;
    std::expected<ports::LoadedManifestBatch, std::string> load_many(
        const std::vector<domain::DirectoryPath>&) override;
    std::expected<void, std::string> save(const domain::DirectoryManifest&) override;

   private:
    TomlManifestMapper mapper_;
};
}  // namespace crumb::infrastructure
