#pragma once
#include "lib/application/reconcile_directory.hpp"
namespace crumb::boundary {
class CommandRouter {
public:
    explicit CommandRouter(application::ReconcileDirectory& reconcile) : reconcile_(reconcile) {}
    int run(int argc, char** argv) const;
private: application::ReconcileDirectory& reconcile_;
};
}
