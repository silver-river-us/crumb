#pragma once

#include "domain/value_objects/value_objects.hpp"
#include <expected>
#include <string>

namespace crumb::ports {
struct MoveOperation {
    domain::DirectoryPath source;
    domain::FileName old_name;
    domain::DirectoryPath destination;
    domain::FileName new_name;
};
class OperationJournal {
public:
    virtual ~OperationJournal() = default;
    virtual std::expected<void, std::string> begin(const MoveOperation&) = 0;
    virtual std::expected<void, std::string> complete() = 0;
    virtual std::expected<void, std::string> recover() = 0;
};
}
