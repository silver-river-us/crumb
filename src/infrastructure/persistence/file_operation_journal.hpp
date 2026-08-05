#pragma once
#include "lib/ports/operation_journal.hpp"
namespace crumb::infrastructure {
class FileOperationJournal final : public ports::OperationJournal {
public:
    std::expected<void, std::string> begin(const ports::MoveOperation&) override { return {}; }
    std::expected<void, std::string> complete() override { return {}; }
    std::expected<void, std::string> recover() override { return {}; }
};
}
