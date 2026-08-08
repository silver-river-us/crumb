#include "application/move_file.hpp"

namespace crumb::application {
std::expected<void, std::string> MoveFile::execute(const domain::DirectoryPath& source, const domain::FileName& old_name,
                                                   const domain::DirectoryPath& destination, domain::FileName new_name) {
    auto source_manifest = manifests_.load(source);
    if (!source_manifest) return std::unexpected(source_manifest.error());
    auto destination_manifest = manifests_.load(destination);
    if (!destination_manifest) return std::unexpected(destination_manifest.error());
    if (!source_manifest.value() || !destination_manifest.value()) return std::unexpected("both manifests must exist");
    auto* entry = source_manifest.value()->find(old_name);
    if (!entry) return std::unexpected("source file is not in manifest");
    if (destination_manifest.value()->find(new_name)) return std::unexpected("destination filename already exists");
    const auto operation = ports::MoveOperation{source, old_name, destination, new_name};
    if (auto journaled = journal_.begin(operation); !journaled) return std::unexpected(journaled.error());
    auto moved = filesystem_.move_file(source, old_name, destination, new_name);
    if (!moved) return std::unexpected(moved.error());
    auto transferred = *entry;
    transferred.name = std::move(new_name);
    source_manifest.value()->remove(old_name);
    destination_manifest.value()->add(std::move(transferred));
    if (auto saved = manifests_.save(*source_manifest.value()); !saved) return std::unexpected(saved.error());
    if (auto saved = manifests_.save(*destination_manifest.value()); !saved) return std::unexpected(saved.error());
    return journal_.complete();
}
}
