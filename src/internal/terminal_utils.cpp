/**
 * ASE CORE INFRASTRUCTURE IMPLEMENTATION
 *
 * @file        terminal_utils.cpp
 * @brief       ANSI colors, timestamps, terminal width detection
 * @description Shared drawing helpers for the boot and shutdown views. This
 *              file WRITES TO A TERMINAL rather than logging: it measures the
 *              window, emits colour codes and formats timestamps so the two
 *              sequence views can draw aligned tables.
 *              Internal implementation detail of ase-ecs.
 *
 * @module      ase-ecs
 * @layer       1 (Core)
 * @category    process/computation
 * @created     2026-02-01
 * @modified    2026-10-05
 * @version     1.1.0
 *
 * CORE INFRASTRUCTURE IMPLEMENTATION COMPLIANCE
 *
 * [ ] NOT an ECS System implementation
 * [ ] Layer dependencies correct (L0: no ASE deps, L1: L0 only)
 * [ ] Own header included FIRST
 * [ ] No global mutable state
 * [ ] No static initialization order fiasco
 * [ ] Thread-safe implementations (pure or mutex-protected)
 * [ ] All error conditions handled
 * [ ] No exceptions thrown (use Result<T> pattern)
 * [ ] Implementation details in anonymous namespace
 * [ ] No inline implementations of template specializations here
 * [ ] Platform-specific code isolated and documented
 * [ ] Performance-critical code profiled and optimized
 */

#include <ase/ecs/internal/terminal_utils.hpp>

#include <ase/utils/clock.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <sys/ioctl.h>
#include <unistd.h>

namespace ase::ecs::internal {

namespace {

// Both timestamp forms are fixed-width by construction: "MM:SS.mmm" is 9 bytes,
// "HH:MM:SS.mmm" is 12. The buffer is sized for the longer one plus the
// terminator, so one constant serves both and neither can grow past it - the
// fields come from a std::tm, whose members are already range-bounded.
constexpr size_t TIMESTAMP_BUFFER_BYTES = 16;

}  // anonymous namespace

void write_terminal(const std::string& text) {
    if (text.empty()) return;
    std::fwrite(text.data(), 1, text.size(), stdout);
}

void flush_terminal() {
    std::fflush(stdout);
}

void draw_terminal(bool render, const std::string& text) {
    if (!render) {
        return;
    }
    write_terminal(text);
    flush_terminal();
}

int get_terminal_width() {
    struct winsize w{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 0) {
        return w.ws_col;
    }
    // Fallback to COLUMNS env var
    if (const char* cols = std::getenv("COLUMNS")) {
        int width = std::atoi(cols);
        if (width > 0) return width;
    }
    return 80;  // Default
}

std::string terminal_line() {
    int width = get_terminal_width();
    std::string line;
    line.reserve(static_cast<size_t>(width) * 3);  // ━ is 3 bytes in UTF-8
    for (int i = 0; i < width; ++i) {
        line += "━";
    }
    return line;
}

/*
 * BOTH TIMESTAMPS READ THE CLOCK ONCE, AND THAT IS THE POINT (2026-08-20).
 *
 * They were built from std::chrono::system_clock: one call for the time_t and a second
 * expression for the millisecond remainder, taken from the same time_point - correct, but
 * spelled with a forbidden source. ase::utils::wall_time_millis() is the same wall clock
 * with the resolution these lines need, and it is read ONCE per timestamp: the seconds and
 * the fraction below come from that single value, so the printed fraction always belongs to
 * the second printed next to it.
 *
 * localtime_r stays. The value is epoch-based UTC; the timestamp on a boot line is meant to
 * be read against the operator's wall clock, and that conversion is what localtime_r is.
 */
std::string short_timestamp() {
    const int64_t now_millis = ase::utils::wall_time_millis();
    const std::time_t time = static_cast<std::time_t>(now_millis / ase::utils::MILLIS_PER_SECOND);
    const int millis = static_cast<int>(now_millis % ase::utils::MILLIS_PER_SECOND);

    std::tm tm_buf{};
    localtime_r(&time, &tm_buf);

    char buffer[TIMESTAMP_BUFFER_BYTES];
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d.%03d",
                  tm_buf.tm_min, tm_buf.tm_sec, millis);
    return std::string(buffer);
}

std::string full_timestamp() {
    const int64_t now_millis = ase::utils::wall_time_millis();
    const std::time_t time = static_cast<std::time_t>(now_millis / ase::utils::MILLIS_PER_SECOND);
    const int millis = static_cast<int>(now_millis % ase::utils::MILLIS_PER_SECOND);

    std::tm tm_buf{};
    localtime_r(&time, &tm_buf);

    char buffer[TIMESTAMP_BUFFER_BYTES];
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d.%03d",
                  tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec, millis);
    return std::string(buffer);
}

size_t source_color_index(const char* source_name) {
    if (!source_name || !*source_name) return 0;

    // Simple hash: djb2
    unsigned long hash = 5381;
    const char* p = source_name;
    while (*p) {
        hash = ((hash << 5) + hash) + static_cast<unsigned char>(*p);
        ++p;
    }

    return hash % SOURCE_COLOR_COUNT;
}

void source_color(const char* source_name, char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size < 16) return;

    size_t idx = source_color_index(source_name);
    int color_code = SOURCE_COLORS[idx];

    std::snprintf(buffer, buffer_size, "\x1b[38;5;%dm", color_code);
}

}  // namespace ase::ecs::internal
