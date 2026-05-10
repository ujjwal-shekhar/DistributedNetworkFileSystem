module;

#include <string_view>

export module client:peek;

import commands;

namespace client::peek {

export commands::Command get_translated_command() {
  return commands::Command::LIST_ALL;
}

} // namespace client::peek
