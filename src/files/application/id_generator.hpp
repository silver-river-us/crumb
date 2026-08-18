#pragma once

#include "files/domain/value_objects/directory_id.hpp"
#include "files/domain/value_objects/file_id.hpp"
namespace crumb::ports {
class IdGenerator {
   public:
    virtual ~IdGenerator() = default;
    virtual domain::DirectoryId directory_id() = 0;
    virtual domain::FileId file_id() = 0;
};
}  // namespace crumb::ports
