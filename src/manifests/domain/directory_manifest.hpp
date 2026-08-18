#pragma once

#include "manifests/domain/file_entry.hpp"
#include "files/domain/value_objects/directory_id.hpp"
#include "files/domain/value_objects/directory_path.hpp"
#include "files/domain/value_objects/file_name.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>
#include <vector>

namespace crumb::domain {

class DirectoryManifest {
   public:
    static DirectoryManifest create(DirectoryId id, DirectoryPath path,
                                    std::string generated_at = {}, std::string generator = {}) {
        return DirectoryManifest(std::move(id), std::move(path), std::move(generated_at),
                                 std::move(generator));
    }

    void add(FileEntry entry) {
        ensure_unique(entry.name);
        files_.push_back(std::move(entry));
        sort_files();
    }
    void remove(const FileName& name) {
        const auto old_size = files_.size();
        std::erase_if(files_,
                      [&](const FileEntry& entry) { return entry.name.value() == name.value(); });
        if (files_.size() == old_size) throw std::out_of_range("file is not in manifest");
    }
    void rename(const FileName& from, FileName to) {
        if (from.value() != to.value()) ensure_unique(to);
        auto* entry = find(from);
        if (entry == nullptr) throw std::out_of_range("file is not in manifest");
        entry->name = std::move(to);
        sort_files();
    }
    void update(FileEntry entry) {
        auto* existing = find(entry.name);
        if (existing == nullptr) throw std::out_of_range("file is not in manifest");
        if (existing->id.value() != entry.id.value())
            throw std::invalid_argument("file identity cannot change on update");
        *existing = std::move(entry);
        sort_files();
    }
    [[nodiscard]] FileEntry* find(const FileName& name) noexcept {
        for (auto& entry : files_)
            if (entry.name.value() == name.value()) return &entry;
        return nullptr;
    }
    [[nodiscard]] const FileEntry* find(const FileName& name) const noexcept {
        for (const auto& entry : files_)
            if (entry.name.value() == name.value()) return &entry;
        return nullptr;
    }
    [[nodiscard]] const std::vector<FileEntry>& files() const noexcept { return files_; }
    [[nodiscard]] std::vector<FileEntry>& files() noexcept { return files_; }
    [[nodiscard]] const DirectoryId& id() const noexcept { return id_; }
    [[nodiscard]] const DirectoryPath& path() const noexcept { return path_; }
    [[nodiscard]] const std::string& generated_at() const noexcept { return generated_at_; }
    void set_generated_at(std::string value) { generated_at_ = std::move(value); }
    [[nodiscard]] const std::string& generator() const noexcept { return generator_; }
    void set_generator(std::string value) { generator_ = std::move(value); }

   private:
    DirectoryManifest(DirectoryId id, DirectoryPath path, std::string generated_at,
                      std::string generator)
        : id_(std::move(id)),
          path_(std::move(path)),
          generated_at_(std::move(generated_at)),
          generator_(std::move(generator)) {}
    void ensure_unique(const FileName& name) const {
        if (find(name) != nullptr) throw std::invalid_argument("duplicate filename in manifest");
    }
    void sort_files() {
        std::ranges::sort(files_, [](const FileEntry& a, const FileEntry& b) {
            return a.name.value() < b.name.value();
        });
    }
    DirectoryId id_;
    DirectoryPath path_;
    std::string generated_at_;
    std::string generator_;
    std::vector<FileEntry> files_;
};
}  // namespace crumb::domain
