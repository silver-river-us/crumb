#pragma once
#include <cstdio>
#include <ostream>
#include "application/index_size.hpp"
#include "application/reconcile_directory.hpp"
#include "application/rebuild_search_index.hpp"
#include "application/search_manifest.hpp"
#include "boundary/cli/user_config.hpp"
#include "plugins/google_drive/google_drive_plugin.hpp"
namespace crumb::boundary {

namespace testing {
using PopenFunction = FILE* (*)(const char*, const char*);
extern PopenFunction popen_function;
std::string relative_result_path_for_test(const application::SearchMatch&,
                                          const domain::DirectoryPath&);
std::string clickable_url_for_test(std::string_view url, bool interactive,
                                   std::string_view label = {});
std::string shorten_for_test(std::string_view value, std::size_t width);
void print_search_table_for_test(std::ostream& output, const domain::DirectoryPath& directory,
                                 const application::SearchResult& result, bool scrollable);
}  // namespace testing

class CommandRouter {
   public:
    CommandRouter(application::ReconcileDirectory& reconcile,
                  application::RebuildSearchIndex& rebuild_index,
                  application::SearchManifest& search, application::IndexSize& index_size,
                  plugins::google_drive::GoogleDrivePlugin& drive, UserConfig config)
        : reconcile_(reconcile),
          rebuild_index_(rebuild_index),
          search_(search),
          index_size_(index_size),
          drive_(drive),
          config_(std::move(config)) {}
    int run(int argc, char** argv) const;

   private:
    application::ReconcileDirectory& reconcile_;
    application::RebuildSearchIndex& rebuild_index_;
    application::SearchManifest& search_;
    application::IndexSize& index_size_;
    plugins::google_drive::GoogleDrivePlugin& drive_;
    UserConfig config_;
};
}  // namespace crumb::boundary
