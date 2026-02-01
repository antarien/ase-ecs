#include <ase/ecs/internal/terminal_utils.hpp>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <sys/ioctl.h>
#include <unistd.h>

namespace ase::ecs::internal {

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

std::string short_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::tm tm_buf{};
    localtime_r(&time, &tm_buf);

    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << tm_buf.tm_min << ":"
        << std::setfill('0') << std::setw(2) << tm_buf.tm_sec << "."
        << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

std::string full_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::tm tm_buf{};
    localtime_r(&time, &tm_buf);

    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << tm_buf.tm_hour << ":"
        << std::setfill('0') << std::setw(2) << tm_buf.tm_min << ":"
        << std::setfill('0') << std::setw(2) << tm_buf.tm_sec << "."
        << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
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
