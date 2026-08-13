#pragma once

#include "domain/value_objects/validated_string.hpp"

#include <stdexcept>
#include <string>
#include <utility>

namespace crumb::domain {

class ContentHash : public ValidatedString {
   public:
    using ValidatedString::ValidatedString;
    static ContentHash create(std::string value) {
        if (value.find(':') == std::string::npos)
            throw std::invalid_argument("content hash needs an algorithm");
        return ContentHash(std::move(value));
    }
    using ValidatedString::operator<=>;
};

}  // namespace crumb::domain
