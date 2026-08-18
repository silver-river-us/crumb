#pragma once

#include "files/domain/value_objects/validated_string.hpp"

#include <stdexcept>
#include <string>
#include <utility>

namespace crumb::domain {

class DirectoryId : public ValidatedString {
   public:
    using ValidatedString::ValidatedString;
    static DirectoryId create(std::string value) {
        if (value.size() != 26) throw std::invalid_argument("directory id must be a ULID");
        return DirectoryId(std::move(value));
    }
    using ValidatedString::operator<=>;
};

}  // namespace crumb::domain
