module;

#include <source_location>
#include <string_view>

export module logger;

export namespace logger {

enum class Level { INFO, WARNING, ERROR, DEBUG };

void log(Level level, std::string_view message,
         const std::source_location location = std::source_location::current());
void info(std::string_view message, const std::source_location location =
                                        std::source_location::current());
void warn(std::string_view message, const std::source_location location =
                                        std::source_location::current());
void error(std::string_view message, const std::source_location location =
                                         std::source_location::current());
void debug(std::string_view message, const std::source_location location =
                                         std::source_location::current());

void set_log_file(std::string_view path);

} // namespace logger
