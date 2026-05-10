module;

#include <filesystem>
#include <string>
#include <string_view>
#include <unistd.h>

export module client:prompt;

import :types;

namespace client::prompt {

export std::string get_prompt(std::string_view logical_cwd,
                              ShellStatus status) {
  char hostname[1024];
  hostname[0] = '\0';
  gethostname(hostname, sizeof(hostname));

  const char *user = std::getenv("USER");
  if (!user)
    user = "user";

  std::string color_prefix = "\033[1;34m"; // BOLD + BLUE
  std::string reset = "\033[0m";

  std::string status_indicator;
  switch (status) {
  case ShellStatus::Success:
    status_indicator = "\033[1;32m^_^/*\033[0m"; // BOLD + GREEN
    break;
  case ShellStatus::Error:
    status_indicator = "\033[1;31mv_v/*\033[0m"; // BOLD + RED
    break;
  case ShellStatus::Warning:
    status_indicator = "\033[1;33m-_-/*\033[0m"; // BOLD + YELLOW
    break;
  }

  return color_prefix + std::string(user) + "@" + std::string(hostname) + ":" +
         std::string(logical_cwd) + reset + " " + status_indicator + " ";
}

} // namespace client::prompt
