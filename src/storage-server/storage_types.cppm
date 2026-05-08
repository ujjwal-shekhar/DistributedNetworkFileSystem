module;

#include <expected>
#include <string>
#include <vector>

export module storage_server:types;

namespace storage {

export enum class Error {
  FileNotFound,
  PermissionDenied,
  AlreadyExists,
  NetworkError,
  OperationFailed
};

export struct Config {
  std::string nm_ip;
  int nm_port;
  int client_port;
  int nm_comm_port;
  std::vector<std::string> accessible_paths;
};

} // namespace storage
