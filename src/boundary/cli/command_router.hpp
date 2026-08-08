#pragma once
#include "application/index_size.hpp"
#include "application/reconcile_directory.hpp"
#include "application/rebuild_search_index.hpp"
#include "application/search_manifest.hpp"
namespace crumb::boundary {
class CommandRouter {
public:
    CommandRouter(application::ReconcileDirectory& reconcile, application::RebuildSearchIndex& rebuild_index,
                  application::SearchManifest& search, application::IndexSize& index_size)
        : reconcile_(reconcile), rebuild_index_(rebuild_index), search_(search), index_size_(index_size) {}
    int run(int argc, char** argv) const;
private:
    application::ReconcileDirectory& reconcile_;
    application::RebuildSearchIndex& rebuild_index_;
    application::SearchManifest& search_;
    application::IndexSize& index_size_;
};
}
