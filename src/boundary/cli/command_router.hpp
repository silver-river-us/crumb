#pragma once
#include "lib/application/reconcile_directory.hpp"
#include "lib/application/search_manifest.hpp"
namespace crumb::boundary {
class CommandRouter {
public:
    CommandRouter(application::ReconcileDirectory& reconcile, application::SearchManifest& search)
        : reconcile_(reconcile), search_(search) {}
    int run(int argc, char** argv) const;
private:
    application::ReconcileDirectory& reconcile_;
    application::SearchManifest& search_;
};
}
