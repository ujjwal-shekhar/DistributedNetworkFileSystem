module;

#include <cstddef>
#include <cstring>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

export module commands;

namespace commands {

export enum class Status { Success, Error, Denied };

export enum class Command {
  FAIL_SERVER,
  CREATE_DIR,
  CREATE_FILE,
  DELETE_DIR,
  DELETE_FILE,
  GET_FILE_INFO,
  READ_FILE,
  WRITE_FILE,
  LIST_ALL,
  REPLICATE_FILE,
  JS_REGISTER,
  JS_HEARTBEAT,
  SUBMIT_JOB,
  EXECUTE_TASK,
};

export enum class PrivilegeTier { USER, PRIVILEGED, ADMIN };

export struct CommandMetadata {
  Command cmd;
  std::string_view name;
  int token_cost;
  PrivilegeTier tier;
};

export constexpr CommandMetadata get_metadata(Command cmd) {
  // FEEDBACK: Can probably use reflection and meta magic to do this
  // Will only need one place to store name, cost, tier info
  // and then lookup at compile time, simple.
  switch (cmd) {
  case Command::FAIL_SERVER:
    return {Command::FAIL_SERVER, "FAIL_SERVER", 0, PrivilegeTier::ADMIN};
  case Command::CREATE_DIR:
    return {Command::CREATE_DIR, "CREATE_DIR", 10, PrivilegeTier::PRIVILEGED};
  case Command::CREATE_FILE:
    return {Command::CREATE_FILE, "CREATE_FILE", 10, PrivilegeTier::PRIVILEGED};
  case Command::DELETE_DIR:
    return {Command::DELETE_DIR, "DELETE_DIR", 20, PrivilegeTier::PRIVILEGED};
  case Command::DELETE_FILE:
    return {Command::DELETE_FILE, "DELETE_FILE", 15, PrivilegeTier::PRIVILEGED};
  case Command::GET_FILE_INFO:
    return {Command::GET_FILE_INFO, "GET_FILE_INFO", 2, PrivilegeTier::USER};
  case Command::READ_FILE:
    return {Command::READ_FILE, "READ_FILE", 5, PrivilegeTier::USER};
  case Command::WRITE_FILE:
    return {Command::WRITE_FILE, "WRITE_FILE", 10, PrivilegeTier::USER};
  case Command::LIST_ALL:
    return {Command::LIST_ALL, "LIST_ALL", 1, PrivilegeTier::USER};
  case Command::REPLICATE_FILE:
    return {Command::REPLICATE_FILE, "REPLICATE_FILE", 0, PrivilegeTier::ADMIN};
  case Command::JS_REGISTER:
    return {Command::JS_REGISTER, "JS_REGISTER", 0, PrivilegeTier::USER};
  case Command::JS_HEARTBEAT:
    return {Command::JS_HEARTBEAT, "JS_HEARTBEAT", 0, PrivilegeTier::USER};
  case Command::SUBMIT_JOB:
    return {Command::SUBMIT_JOB, "SUBMIT_JOB", 50, PrivilegeTier::USER};
  case Command::EXECUTE_TASK:
    return {Command::EXECUTE_TASK, "EXECUTE_TASK", 0, PrivilegeTier::USER};
  }
  return {Command::LIST_ALL, "UNKNOWN", 999, PrivilegeTier::USER};
}

export [[nodiscard]] constexpr std::optional<Command>
string_to_command(std::string_view s) {
  // FEEDBACK: Similar to above, can probably use reflection/meta to generate
  // this at compile time
  if (s == "FAIL_SERVER")
    return Command::FAIL_SERVER;
  if (s == "CREATE_DIR")
    return Command::CREATE_DIR;
  if (s == "CREATE_FILE")
    return Command::CREATE_FILE;
  if (s == "DELETE_DIR")
    return Command::DELETE_DIR;
  if (s == "DELETE_FILE")
    return Command::DELETE_FILE;
  if (s == "GET_FILE_INFO")
    return Command::GET_FILE_INFO;
  if (s == "READ_FILE")
    return Command::READ_FILE;
  if (s == "WRITE_FILE")
    return Command::WRITE_FILE;
  if (s == "LIST_ALL" || s == "LIST_FILES")
    return Command::LIST_ALL;
  if (s == "SUBMIT_JOB" || s == "job")
    return Command::SUBMIT_JOB;
  return std::nullopt;
}

export struct Config {
  std::string nm_ip = "127.0.0.1";
  int nm_clt_port = 8080;
  int nm_ss_reg_port = 5049;
  int nm_ss_comm_port = 4050;
  int nm_js_reg_port = 6051; // New port for Job Server registration
};

export struct ClientDetails {
  int id = -1;
};

export constexpr size_t MAX_ARG_LEN = 256;
export constexpr size_t MAX_ARGS_COUNT = 16;
export constexpr size_t MAX_PATH_LEN = 256;
export constexpr size_t MAX_PATHS = 10;
export constexpr size_t MAX_IP_LEN = 64;
export constexpr size_t FILE_CHUNK_SIZE = 4096;

export struct ClientRequest {
  ClientDetails client;
  Command command;
  char arg1[MAX_ARG_LEN];
  char arg2[MAX_ARG_LEN];
};

export struct JobPayload {
  char command_line[MAX_ARG_LEN];
  char target_file[MAX_PATH_LEN];
};

export struct ServerDetails {
  int id = -1;
  char ip[MAX_IP_LEN];
  int port_nm = 0;
  int port_client = 0;
  char paths[MAX_PATHS][MAX_PATH_LEN];
  int path_count = 0;
  bool online = false;
};

export struct AckPacket {
  Status status = Status::Success;
  int error_code = 0;
  int extra_info[8]; // Fixed size instead of vector
  int extra_count = 0;
};

export struct FilePacket {
  char chunk[FILE_CHUNK_SIZE];
  size_t size = 0;
  bool is_last = false;
};

export template <typename T>
[[nodiscard]] std::vector<std::byte> serialize(const T &obj) {
  return {};
}
} // namespace commands
