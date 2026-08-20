#pragma once

#include "files/application/filesystem.hpp"

#include <string>

namespace crumb::infrastructure {
class NativeFileSystem;
}  // namespace crumb::infrastructure

namespace crumb::plugins::google_drive {

class DriveFileSystem final : public ports::FileSystem {
   public:
    explicit DriveFileSystem(infrastructure::NativeFileSystem& native) : native_(native) {}

    std::expected<std::vector<domain::FileSnapshot>, std::string> list_regular_files(
        const domain::DirectoryPath&) override;
    std::expected<std::optional<std::string>, std::string> read_text_file(
        const domain::DirectoryPath&, const domain::FileName&) override;
    std::expected<std::vector<std::pair<domain::DirectoryPath, domain::FileName>>, std::string>
    list_regular_files_recursive(const domain::DirectoryPath&) override;
    std::expected<std::vector<domain::DirectoryPath>, std::string> list_directories_recursive(
        const domain::DirectoryPath&) override;

   private:
    infrastructure::NativeFileSystem& native_;
};

}  // namespace crumb::plugins::google_drive
