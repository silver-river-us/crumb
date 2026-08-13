#pragma once

#include "domain/value_objects/validated_string.hpp"

#include <stdexcept>
#include <string>
#include <utility>

namespace crumb::domain {

class FileName : public ValidatedString {
   public:
    using ValidatedString::ValidatedString;
    static FileName create(std::string value) {
        if (value.empty() || value == "." || value == ".." ||
            value.find('/') != std::string::npos || value.find('\\') != std::string::npos) {
            throw std::invalid_argument("invalid immediate filename");
        }
        return FileName(std::move(value));
    }
    using ValidatedString::operator<=>;
};

}  // namespace crumb::domain
