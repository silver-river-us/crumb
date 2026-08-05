#pragma once

#include "domain/value_objects/value_objects.hpp"
namespace crumb::ports {
class IdGenerator {
public:
    virtual ~IdGenerator() = default;
    virtual domain::DirectoryId directory_id() = 0;
    virtual domain::FileId file_id() = 0;
};
}
