module;

#include <iostream>
#include <print>
#include <source_location>
#include <string_view>

module logger;

import core;

namespace logger {

void log(Level level, std::string_view message,
         const std::source_location location) {
  std::string_view level_str;
  switch (level) {
  case Level::INFO:
    level_str = "[INFO] ";
    break;
  case Level::WARNING:
    level_str = "[WARN] ";
    break;
  case Level::ERROR:
    level_str = "[ERROR]";
    break;
  case Level::DEBUG:
    level_str = "[DEBUG]";
    break;
  }

  if (level == Level::ERROR) {
    std::println(std::cerr, "{} {}:{} | {}", level_str, location.file_name(),
                 location.line(), message);
  } else {
    std::println("{} {}:{} | {}", level_str, location.file_name(),
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
