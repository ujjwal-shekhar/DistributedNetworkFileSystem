module;

#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
import commands;

export module naming_server:commands;

namespace naming {

export enum class Command {
  FAIL_SERVER,
  CREATE_DIR,
  CREATE_FILE,
  DELETE_DIR,
  DELETE_FILE,
  GET_FILE_INFO,
  READ_FILE,
  WRITE_FILE,
  LIST_ALL
};

export enum class PrivilegeTier { USER, PRIVILEGED, ADMIN };

export struct CommandMetadata {
  Command cmd;
  std::string_view name;
  int token_cost;
  PrivilegeTier tier;
};

export constexpr CommandMetadata get_metadata(Command cmd) {
  auto meta = commands::get_metadata(static_cast<commands::Command>(cmd));
  return {cmd, meta.name, meta.token_cost,
          static_cast<PrivilegeTier>(meta.tier)};
}

export [[nodiscard]] constexpr std::optional<Command>
string_to_command(std::string_view s) {
  auto res = commands::string_to_command(s);
  if (!res)
    return std::nullopt;
  return static_cast<Command>(*res);
}

} // namespace naming
