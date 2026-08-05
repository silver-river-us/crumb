#pragma once
#include "lib/ports/manifest_repository.hpp"
#include "lib/ports/operation_journal.hpp"
#include "lib/ports/filesystem.hpp"
#include <expected>
#include <string>
namespace crumb::application {
class MoveFile {
public:
    MoveFile(ports::ManifestRepository& manifests, ports::FileSystem& filesystem, ports::OperationJournal& journal)
        : manifests_(manifests), filesystem_(filesystem), journal_(journal) {}
    std::expected<void, std::string> execute(const domain::DirectoryPath&, const domain::FileName&,
                                             const domain::DirectoryPath&, domain::FileName);
private:
    ports::ManifestRepository& manifests_; ports::FileSystem& filesystem_; ports::OperationJournal& journal_;
};
}
