#pragma once

#include <cstdio>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>

#include "presentation/cli/user_config.hpp"

namespace crumb::application {
class IndexSize;
class RebuildSearchIndex;
class ReconcileDirectory;
class SearchManifest;
struct SearchMatch;
struct SearchResult;
}  // namespace crumb::application
namespace crumb::domain {
class DirectoryPath;
}
namespace crumb::plugins::google_drive {
class GoogleDrivePlugin;
}

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
    int run_index(int argc, char** argv) const;
    int run_index_size(int argc, char** argv) const;
    int run_scan(const domain::DirectoryPath& directory) const;
    int run_search(int argc, char** argv) const;

    application::ReconcileDirectory& reconcile_;
    application::RebuildSearchIndex& rebuild_index_;
    application::SearchManifest& search_;
    application::IndexSize& index_size_;
    plugins::google_drive::GoogleDrivePlugin& drive_;
    UserConfig config_;
};
}  // namespace crumb::boundary
