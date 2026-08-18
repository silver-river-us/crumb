#include "boundary/cli/search_tap.hpp"

#include "boundary/cli/internal/search_tap_internal.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace crumb::boundary::detail {
std::string format_result_date(std::optional<std::int64_t> nanoseconds, LocalTime local_time) {
    if (!nanoseconds) return "-";
    const auto seconds = static_cast<std::time_t>(*nanoseconds / 1'000'000'000);
    const auto* local = local_time(&seconds);
    char formatted[11]{};
    if (local && std::strftime(formatted, sizeof formatted, "%Y-%m-%d", local) != 0)
        return formatted;
    return "-";
}

std::uint64_t total_duration(const application::SearchResult& result) {
    std::uint64_t total{};
    for (const auto& span : result.trace)
        total = std::max(total, span.offset_us + span.duration_us);
    return total;
}

std::string format_duration(std::uint64_t microseconds) {
    std::ostringstream output;
    if (microseconds < 1'000)
        output << microseconds << " μs";
    else if (microseconds < 1'000'000)
        output << std::fixed << std::setprecision(microseconds < 10'000 ? 2 : 1)
               << static_cast<double>(microseconds) / 1'000.0 << " ms";
    else if (microseconds < 1'000'000'000)
        output << std::fixed << std::setprecision(2)
               << static_cast<double>(microseconds) / 1'000'000.0 << " s";
    else
        output << std::fixed << std::setprecision(2)
               << static_cast<double>(microseconds) / 1'000'000'000.0 << " s";
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
        const auto offset =
            total == 0
                ? std::size_t{0}
                : std::min(bar_width, static_cast<std::size_t>(span.offset_us * bar_width / total));
        const auto available = std::max<std::size_t>(1, bar_width - offset);
        const auto bar =
            total == 0
                ? std::size_t{1}
                : std::min(available,
                           std::max<std::size_t>(
                               1, static_cast<std::size_t>(span.duration_us * bar_width / total)));
        output << "  " << std::left << std::setw(static_cast<int>(name_width)) << span.name << " |"
               << std::string(offset, ' ') << std::string(bar, '#')
               << std::string(bar_width - offset - bar, ' ') << "| " << std::right << std::setw(9)
               << format_duration(span.duration_us) << " @ " << std::setw(9)
               << format_duration(span.offset_us);
        if (!span.detail.empty()) output << "  " << span.detail;
        output << '\n';
    }
    return output.str();
}
}  // namespace crumb::boundary::detail

namespace crumb::boundary {
std::string format_result_date_for_test(std::optional<std::int64_t> nanoseconds,
                                        std::tm* (*local_time)(const std::time_t*)) {
    return detail::format_result_date(nanoseconds, local_time);
}

std::string render_search_tap(std::string_view query, const domain::DirectoryPath& directory,
                              const application::SearchResult& result, TapFormat format) {
    return format == TapFormat::html ? detail::render_html(query, directory, result)
                                     : detail::render_text(query, directory, result);
}
}  // namespace crumb::boundary
