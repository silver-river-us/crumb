#pragma once

#include "domain/value_objects/validated_string.hpp"

#include <stdexcept>
#include <string>
#include <utility>

namespace crumb::domain {

class DirectoryPath : public ValidatedString {
   public:
    using ValidatedString::ValidatedString;
    static DirectoryPath create(std::string value) {
        if (value.empty()) throw std::invalid_argument("directory path must not be empty");
        return DirectoryPath(std::move(value));
    }
    using ValidatedString::operator<=>;
};

}  // namespace crumb::domain
