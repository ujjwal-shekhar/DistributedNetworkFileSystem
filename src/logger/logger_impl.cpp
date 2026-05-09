module;

#include <iostream>
#include <print>
#include <source_location>
#include <string_view>
#include <chrono>
#include <format>

module logger;

namespace logger {

std::string_view level_to_string(Level level) {
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
  std::string timestamp = std::format("{:%Y-%m-%d %H:%M:%S}", std::chrono::floor<std::chrono::seconds>(now));

  std::string_view level_str = level_to_string(level);
  std::string_view file = location.file_name();
  
  if (auto pos = file.find("src/"); pos != std::string_view::npos) {
    file = file.substr(pos + 4);
  }

  if (level == Level::ERROR) {
    std::println(std::cerr, "[{}] {} {}:{} | {}", timestamp, level_str, file,
                 location.line(), message);
  } else {
    std::println("[{}] {} {}:{} | {}", timestamp, level_str, file,
                 location.line(), message);
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

} // namespace logger
