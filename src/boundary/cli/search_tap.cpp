#include "boundary/cli/search_tap.hpp"

#include <algorithm>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <sstream>

namespace crumb::boundary {
using LocalTime = std::tm* (*)(const std::time_t*);

std::string format_result_date(std::optional<std::int64_t> nanoseconds,
                               LocalTime local_time = std::localtime) {
    if (!nanoseconds) return "-";
    const auto seconds = static_cast<std::time_t>(*nanoseconds / 1'000'000'000);
    const auto* local = local_time(&seconds);
    char formatted[11]{};
    if (local && std::strftime(formatted, sizeof formatted, "%Y-%m-%d", local) != 0)
        return formatted;
    return "-";
}

namespace {

std::uint64_t total_duration(const application::SearchResult& result) {
    std::uint64_t total{};
    for (const auto& span : result.trace) {
        total = std::max(total, span.offset_us + span.duration_us);
    }
    return total;
}

std::string format_duration(std::uint64_t microseconds) {
    std::ostringstream output;
    if (microseconds < 1'000) {
        output << microseconds << " μs";
    } else if (microseconds < 1'000'000) {
        output << std::fixed << std::setprecision(microseconds < 10'000 ? 2 : 1)
               << static_cast<double>(microseconds) / 1'000.0 << " ms";
    } else if (microseconds < 1'000'000'000) {
        output << std::fixed << std::setprecision(2)
               << static_cast<double>(microseconds) / 1'000'000.0 << " s";
    } else {
        output << std::fixed << std::setprecision(2)
               << static_cast<double>(microseconds) / 1'000'000'000.0 << " s";
    }
    return output.str();
}

std::string text_escape(std::string_view value) {
    std::string escaped;
    for (const char character : value) {
        if (character == '\\')
            escaped += "\\\\";
        else if (character == '"')
            escaped += "\\\"";
        else if (character == '\n')
            escaped += "\\n";
        else if (character == '\r')
            escaped += "\\r";
        else
            escaped += character;
    }
    return escaped;
}

std::string html_escape(std::string_view value) {
    std::string escaped;
    for (const char character : value) {
        switch (character) {
            case '&':
                escaped += "&amp;";
                break;
            case '<':
                escaped += "&lt;";
                break;
            case '>':
                escaped += "&gt;";
                break;
            case '"':
                escaped += "&quot;";
                break;
            case '\'':
                escaped += "&#39;";
                break;
            default:
                escaped += character;
                break;
        }
    }
    return escaped;
}

std::string render_text(std::string_view query, const domain::DirectoryPath& directory,
                        const application::SearchResult& result) {
    constexpr std::size_t bar_width = 48;
    const auto total = total_duration(result);
    std::size_t name_width = 0;
    for (const auto& span : result.trace) name_width = std::max(name_width, span.name.size());

    std::ostringstream output;
    output << "tap query=\"" << text_escape(query) << "\" directory=\""
           << text_escape(directory.value()) << "\" total=" << format_duration(total) << '\n';
    for (const auto& span : result.trace) {
        const auto offset_size =
            total == 0
                ? std::size_t{}
                : std::min(bar_width, static_cast<std::size_t>(span.offset_us * bar_width / total));
        const auto available_width = std::max<std::size_t>(1, bar_width - offset_size);
        const auto bar_size =
            total == 0 ? std::size_t{1}
                       : std::min(available_width,
                                  std::max<std::size_t>(1, span.duration_us * bar_width / total));
        output << "  " << std::left << std::setw(static_cast<int>(name_width)) << span.name << " |"
               << std::string(offset_size, ' ') << std::string(bar_size, '#')
               << std::string(bar_width - offset_size - bar_size, ' ') << "| " << std::right
               << std::setw(9) << format_duration(span.duration_us) << " @ " << std::setw(9)
               << format_duration(span.offset_us);
        if (!span.detail.empty()) output << "  " << span.detail;
        output << '\n';
    }
    return output.str();
}

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
           "border-bottom:1px solid "
           "#303744;text-align:left}code{color:#b9d4ff}</style></head><body><main>";
    output << "<h1>Crumb search tap</h1><div class=\"meta\">query <code>" << html_escape(query)
           << "</code> · directory <code>" << html_escape(directory.value()) << "</code> · total "
           << format_duration(total) << " · " << result.matches.size() << " matches</div>";
    output << "<section aria-label=\"Search waterfall\">";
    for (const auto& span : result.trace) {
        const auto offset =
            total == 0 ? 0.0
                       : 100.0 * static_cast<double>(span.offset_us) / static_cast<double>(total);
        const auto width =
            total == 0 ? 0.0
                       : 100.0 * static_cast<double>(span.duration_us) / static_cast<double>(total);
        output << "<div class=\"row\"><div class=\"name\">" << html_escape(span.name)
               << "</div><div class=\"track\"><div class=\"bar\" style=\"width:" << std::fixed
               << std::setprecision(2) << width << "%;left:" << offset << "%\"></div></div><div>"
               << format_duration(span.duration_us) << "</div><div>@ "
               << format_duration(span.offset_us) << "</div></div>";
    }
    output << "</section><table><thead><tr><th>ID</th><th>Result</th><th>Score</th><th>Type</"
              "th><th>Created</th><th>Edited</th><th>Link</th></tr></thead><tbody>";
    for (const auto& match : result.matches) {
        const auto path =
            (std::filesystem::path(match.directory.value()) / match.name.value()).string();
        output << "<tr><td><code>" << html_escape(match.file_id.value_or("-"))
               << "</code></td><td><code>" << html_escape(path) << "</code></td><td>" << std::fixed
               << std::setprecision(4) << match.score << "</td><td>" << html_escape(match.type)
               << "</td><td>";
        output << format_result_date(match.created_ns);
        output << "</td><td>";
        output << format_result_date(match.modified_ns);
        output << "</td><td>";
        if (match.external_url) {
            output << "<a href=\"" << html_escape(*match.external_url) << "\">open</a>";
        }
        output << "</td></tr>";
    }
    output << "</tbody></table></main></body></html>\n";
    return output.str();
}

}  // namespace

std::string format_result_date_for_test(std::optional<std::int64_t> nanoseconds,
                                        std::tm* (*local_time)(const std::time_t*)) {
    return format_result_date(nanoseconds, local_time);
}

std::string render_search_tap(std::string_view query, const domain::DirectoryPath& directory,
                              const application::SearchResult& result, TapFormat format) {
    return format == TapFormat::html ? render_html(query, directory, result)
                                     : render_text(query, directory, result);
}

}  // namespace crumb::boundary
