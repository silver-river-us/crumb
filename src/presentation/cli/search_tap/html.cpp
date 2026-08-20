#include <_time.h>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "presentation/cli/search_tap/internal.hpp"
#include "files/domain/value_objects/directory_path.hpp"
#include "files/domain/value_objects/file_name.hpp"
#include "search/application/search_manifest/match.hpp"
#include "search/application/search_manifest/result.hpp"
#include "search/application/search_manifest/trace.hpp"

namespace crumb::boundary::detail {
std::string render_html(std::string_view query, const domain::DirectoryPath& directory,
                        const application::SearchResult& result) {
    const auto total = total_duration(result);
    std::ostringstream output;
    output
        << "<!doctype html>\n<html lang=\"en\"><head><meta charset=\"utf-8\">"
           "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
           "<title>Crumb search tap</title><style>"
           ":root{color-scheme:light dark}body{font:14px system-ui,sans-serif;margin:2rem;"
           "background:#10131a;color:#e8edf5}main{max-width:1100px;margin:auto}"
           "h1{font-size:1.35rem}.meta{color:#aeb9c9;margin-bottom:1.5rem}"
           ".row{display:grid;grid-template-columns:150px 1fr 90px 90px;gap:12px;"
           "align-items:center;margin:.5rem 0}.name{font-family:ui-monospace,monospace}"
           ".track{height:20px;background:#252b36;border-radius:4px;overflow:hidden;position:"
           "relative}"
           ".bar{height:100%;background:#4f8cff;border-radius:4px;min-width:2px;position:absolute}"
           "table{border-collapse:collapse;width:100%;margin-top:2rem}th,td{padding:.55rem;"
           "border-bottom:1px solid #303744;text-align:left}code{color:#b9d4ff}</style></head>"
           "<body><main>";
    output << "<h1>Crumb search tap</h1><div class=\"meta\">query <code>" << html_escape(query)
           << "</code> · directory <code>" << html_escape(directory.value()) << "</code> · total "
           << format_duration(total) << " · " << result.matches.size()
           << " matches</div><section aria-label=\"Search waterfall\">";
    for (const auto& span : result.trace) {
        const auto total_duration_us = static_cast<double>(total);
        const auto offset =
            total == 0 ? 0.0 : 100.0 * static_cast<double>(span.offset_us) / total_duration_us;
        const auto width =
            total == 0 ? 0.0 : 100.0 * static_cast<double>(span.duration_us) / total_duration_us;
        output << "<div class=\"row\"><div class=\"name\">" << html_escape(span.name)
               << "</div><div class=\"track\"><div class=\"bar\" style=\"width:" << std::fixed
               << std::setprecision(2) << width << "%;left:" << offset << "%\"></div></div><div>"
               << format_duration(span.duration_us) << "</div><div>@ "
               << format_duration(span.offset_us) << "</div></div>";
    }
    output << "</section><table><thead><tr><th>ID</th><th>Result</th><th>Score</th><th>Type</th>"
              "<th>Created</th><th>Edited</th><th>Link</th></tr></thead><tbody>";
    for (const auto& match : result.matches) {
        const auto path =
            (std::filesystem::path(match.directory.value()) / match.name.value()).string();
        output << "<tr><td><code>" << html_escape(match.file_id.value_or("-"))
               << "</code></td><td><code>" << html_escape(path) << "</code></td><td>" << std::fixed
               << std::setprecision(4) << match.score << "</td><td>" << html_escape(match.type)
               << "</td><td>" << format_result_date(match.created_ns, std::localtime) << "</td><td>"
               << format_result_date(match.modified_ns, std::localtime) << "</td><td>";
        if (match.external_url)
            output << "<a href=\"" << html_escape(*match.external_url) << "\">open</a>";
        output << "</td></tr>";
    }
    output << "</tbody></table></main></body></html>\n";
    return output.str();
}
}  // namespace crumb::boundary::detail
