module;

#include <cstdlib>
#include <expected>
#include <filesystem>
#include <string_view>

export module client:warp;

import :utils;

namespace client::warp {

export std::string execute(std::string_view current_cwd,
                           std::string_view input_path) {
  if (input_path.empty() || input_path == "~") {
    return "/";
  }

  return utils::resolve_path(current_cwd, input_path);
}

} // namespace client::warp
