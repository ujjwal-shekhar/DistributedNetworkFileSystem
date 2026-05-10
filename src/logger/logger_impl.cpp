module;

#include <chrono>
#include <format>
#include <fstream>
#include <iostream>
#include <mutex>
#include <print>
#include <source_location>
#include <string_view>

module logger;

namespace logger {

static std::ofstream log_file_stream;
static std::mutex log_mutex;

std::string_view level_to_string(Level level) {
  using namespace std::literals;
  switch (level) {
  case Level::INFO:
    return " [INFO]  ";
  case Level::WARNING:
    return " [WARN]  ";
  case Level::ERROR:
    return " [ERROR] ";
  case Level::DEBUG:
    return " [DEBUG] ";
  }
  return "[UNKNOWN]";
}

std::string_view level_to_string_ansi(Level level) {
  using namespace std::literals;
  switch (level) {
  case Level::INFO:
    return "\033[34m [INFO]  \033[0m";
  case Level::WARNING:
    return "\033[33m [WARN]  \033[0m";
  case Level::ERROR:
    return "\033[31m [ERROR] \033[0m";
  case Level::DEBUG:
    return "\033[32m [DEBUG] \033[0m";
  }
  return "[UNKNOWN]";
}

void log(Level level, std::string_view message,
         const std::source_location location) {
  auto now = std::chrono::system_clock::now();
  std::string timestamp = std::format(
      "{:%Y-%m-%d %H:%M:%S}", std::chrono::floor<std::chrono::seconds>(now));

  std::string_view file = location.file_name();
  if (auto pos = file.find("src/"); pos != std::string_view::npos) {
    file = file.substr(pos + 4);
  }

  std::lock_guard<std::mutex> lock(log_mutex);
  if (log_file_stream.is_open()) {
    log_file_stream << std::format("[{}] {} {}:{} | {}\n", timestamp,
                                   level_to_string(level), file,
                                   location.line(), message)
                    << std::flush;
  } else {
    std::string_view level_str = level_to_string_ansi(level);
    if (level == Level::ERROR) {
      std::println(std::cerr, "[{}] {} {}:{} | {}", timestamp, level_str, file,
                   location.line(), message);
    } else {
      std::println("[{}] {} {}:{} | {}", timestamp, level_str, file,
                   location.line(), message);
    }
  }
}

void info(std::string_view message, const std::source_location location) {
  log(Level::INFO, message, location);
}

void warn(std::string_view message, const std::source_location location) {
  log(Level::WARNING, message, location);
}

void error(std::string_view message, const std::source_location location) {
  log(Level::ERROR, message, location);
}

void debug(std::string_view message, const std::source_location location) {
  log(Level::DEBUG, message, location);
}

void set_log_file(std::string_view path) {
  std::lock_guard<std::mutex> lock(log_mutex);
  if (log_file_stream.is_open()) {
    log_file_stream.close();
  }
  log_file_stream.open(std::string(path), std::ios::app);
}

} // namespace logger
