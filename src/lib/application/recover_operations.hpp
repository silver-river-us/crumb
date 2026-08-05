#pragma once
#include "lib/ports/operation_journal.hpp"
#include <expected>
#include <string>
namespace crumb::application {
class RecoverOperations {
public:
    explicit RecoverOperations(ports::OperationJournal& journal) : journal_(journal) {}
    std::expected<void, std::string> execute() { return journal_.recover(); }
private: ports::OperationJournal& journal_;
};
}
