#pragma once
#include "lib/ports/id_generator.hpp"
#include <random>
namespace crumb::infrastructure {
class UlidGenerator final : public ports::IdGenerator {
public:
    UlidGenerator();
    domain::DirectoryId directory_id() override;
    domain::FileId file_id() override;
private:
    std::mt19937_64 random_;
    std::string generate();
};
}
