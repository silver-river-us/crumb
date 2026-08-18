#pragma once

#include "files/domain/value_objects/validated_string.hpp"

#include <stdexcept>
#include <string>
#include <utility>

namespace crumb::domain {

class FileId : public ValidatedString {
   public:
    using ValidatedString::ValidatedString;
    static FileId create(std::string value) {
        if (value.size() != 26) throw std::invalid_argument("file id must be a ULID");
        return FileId(std::move(value));
    }
    using ValidatedString::operator<=>;
};

}  // namespace crumb::domain
