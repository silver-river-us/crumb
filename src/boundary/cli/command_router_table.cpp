#include "boundary/cli/internal/command_router_internal.hpp"

#include <cstdio>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <vector>
#include <unistd.h>

namespace crumb::boundary {
namespace testing {
PopenFunction popen_function = ::popen;
}  // namespace testing

namespace detail {
void print_search_table(std::ostream& output, const domain::DirectoryPath& directory,
                        const application::SearchResult& result, bool scrollable) {
    std::ostringstream table;
    const bool interactive = scrollable && ::isatty(STDOUT_FILENO) == 1;
    const std::vector<std::size_t> widths{3, 20, 50, 46, 9, 20, 10, 10, 60};
    const auto rule = [&table, &widths](char fill) {
        table << '+';
        for (const auto width : widths) table << std::string(width + 2, fill) << '+';
        table << '\n';
    };
    const auto row = [&table, &widths, interactive](const std::vector<std::string>& values) {
        table << '|';
        for (std::size_t index = 0; index < widths.size(); ++index) {
            const auto value = index < values.size() ? values[index] : "";
            const auto rendered = index == widths.size() - 1 && value != "-"
                                      ? clickable_url(value, interactive)
                                      : value;
            table << ' ' << rendered
                  << std::string(value.size() < widths[index] ? widths[index] - value.size() : 0,
                                 ' ')
                  << " |";
        }
        table << '\n';
    };
    table << "Search results: " << result.matches.size() << " matches in " << directory.value()
          << '\n';
    rule('-');
    row({"#", "ID", "Name", "Folder", "Score", "Type", "Created", "Edited", "Link"});
    rule('-');
    for (std::size_t index = 0; index < result.matches.size(); ++index) {
        const auto& match = result.matches[index];
        const auto path = std::filesystem::path(relative_result_path(directory, match));
        std::ostringstream score;
        score << std::fixed << std::setprecision(4) << match.score;
        row({std::to_string(index + 1), match.file_id.value_or("-"),
             shorten(path.filename().string(), widths[2]),
             shorten(path.parent_path().string(), widths[3]), score.str(),
             shorten(match.type.empty() ? path.extension().string() : match.type, widths[5]),
             format_date(match.created_ns), format_date(match.modified_ns),
             match.external_url.value_or("-")});
    }
    rule('-');
    const auto rendered = table.str();
    if (!scrollable || !interactive) {
        output << rendered;
        return;
    }
    if (FILE* pager = testing::popen_function("less -SRFX", "w")) {
        (void)::fwrite(rendered.data(), 1, rendered.size(), pager);
        (void)::pclose(pager);
        return;
    }
    output << rendered;
}
}  // namespace detail

namespace testing {
std::string relative_result_path_for_test(const application::SearchMatch& match,
                                          const domain::DirectoryPath& root) {
    return detail::relative_result_path(root, match);
}
std::string clickable_url_for_test(std::string_view url, bool interactive, std::string_view label) {
    return detail::clickable_url(url, interactive, label);
}
std::string shorten_for_test(std::string_view value, std::size_t width) {
    return detail::shorten(value, width);
}
void print_search_table_for_test(std::ostream& output, const domain::DirectoryPath& directory,
                                 const application::SearchResult& result, bool scrollable) {
    detail::print_search_table(output, directory, result, scrollable);
}
}  // namespace testing
}  // namespace crumb::boundary
