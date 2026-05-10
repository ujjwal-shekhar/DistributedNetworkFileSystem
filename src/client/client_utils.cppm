module;

#include <algorithm>
#include <filesystem>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

export module client:utils;

namespace client::utils {

export std::string resolve_path(std::string_view current_cwd,
                                std::string_view input_path) {
  std::filesystem::path p;
  if (input_path.starts_with("/")) {
    p = input_path;
  } else {
    p = std::filesystem::path(current_cwd) / input_path;
  }

  // Lexically normalize to handle .. and .
  p = p.lexically_normal();

  std::string result = p.string();
  if (result.empty())
    return "/";
  if (result[0] != '/')
    result = "/" + result;

  // Simple check to prevent escaping root
  if (result.find("/..") == 0)
    return "/";

  return result;
}

export std::vector<std::string> split_commands(std::string_view input) {
  std::vector<std::string> commands;
  std::string current;

  auto is_delimiter = [](char c) { return c == ';' || c == '&'; };

  for (size_t i = 0; i < input.size(); ++i) {
    if (is_delimiter(input[i])) {
      // Handle &&
      if (input[i] == '&' && i + 1 < input.size() && input[i + 1] == '&') {
        i++;
      }
      if (!current.empty()) {
        commands.push_back(current);
        current.clear();
      }
    } else {
      current += input[i];
    }
  }

  if (!current.empty()) {
    commands.push_back(current);
  }

  // Trim whitespace
  for (auto &cmd : commands) {
    auto first = cmd.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) {
      cmd.clear();
      continue;
    }
    auto last = cmd.find_last_not_of(" \t\n\r");
    cmd = cmd.substr(first, (last - first + 1));
  }

  commands.erase(std::remove_if(commands.begin(), commands.end(),
                                [](const std::string &s) { return s.empty(); }),
                 commands.end());

  return commands;
}

export bool contains_pipe(std::string_view input) {
  return input.find('|') != std::string_view::npos;
}

export bool contains_redirect(std::string_view input) {
  return input.find('>') != std::string_view::npos ||
         input.find('<') != std::string_view::npos;
}

} // namespace client::utils
