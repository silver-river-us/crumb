#pragma once

#include "files/domain/value_objects/validated_string.hpp"

#include <stdexcept>
#include <string>
#include <utility>

namespace crumb::domain {

class Fingerprint : public ValidatedString {
   public:
    using ValidatedString::ValidatedString;
    static Fingerprint create(std::string value) {
        if (value.find(':') == std::string::npos)
            throw std::invalid_argument("fingerprint needs an algorithm");
        return Fingerprint(std::move(value));
    }
    using ValidatedString::operator<=>;
};

}  // namespace crumb::domain
