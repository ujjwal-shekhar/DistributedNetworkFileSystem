module;

#include <algorithm>
#include <filesystem>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

export module client:utils;

import commands;

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

  // Trim trailing slash if not root
  if (result.size() > 1 && result.back() == '/') {
    result.pop_back();
  }

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

export bool contains_async(std::string_view input) {
  return input.find('&') != std::string_view::npos;
}

export bool contains_redirect(std::string_view input) {
  return input.find('>') != std::string_view::npos ||
         input.find('<') != std::string_view::npos;
}

export commands::DAGPayload parse_dag(std::string_view input,
                                      std::string_view cwd);

export std::vector<std::string> split_args(std::string_view input) {
  std::vector<std::string> args;
  std::string current;
  bool in_quotes = false;
  char quote_char = 0;

  for (size_t i = 0; i < input.size(); ++i) {
    char c = input[i];
    if ((c == '"' || c == '\'') && (i == 0 || input[i - 1] != '\\')) {
      if (in_quotes && c == quote_char) {
        in_quotes = false;
      } else if (!in_quotes) {
        in_quotes = true;
        quote_char = c;
      } else {
        current += c;
      }
    } else if (std::isspace(c) && !in_quotes) {
      if (!current.empty()) {
        args.push_back(current);
        current.clear();
      }
    } else {
      current += c;
    }
  }
  if (!current.empty()) {
    args.push_back(current);
  }
  return args;
}

} // namespace client::utils
