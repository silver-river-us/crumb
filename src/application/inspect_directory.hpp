#pragma once
#include "lib/ports/filesystem.hpp"
namespace crumb::application {
class InspectDirectory {
public:
    explicit InspectDirectory(ports::FileSystem& filesystem) : filesystem_(filesystem) {}
    auto execute(const domain::DirectoryPath& directory) { return filesystem_.list_regular_files(directory); }
private: ports::FileSystem& filesystem_;
};
}
