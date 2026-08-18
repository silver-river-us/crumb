#pragma once

#include <string>
#include <utility>

namespace crumb::domain {

class ValidatedString {
   public:
    ValidatedString() = default;
    explicit ValidatedString(std::string value) : value_(std::move(value)) {}
    [[nodiscard]] const std::string& value() const noexcept { return value_; }
    [[nodiscard]] bool empty() const noexcept { return value_.empty(); }
    auto operator<=>(const ValidatedString&) const = default;

   protected:
    std::string value_;
};

}  // namespace crumb::domain
