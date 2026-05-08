module;

#include <expected>
#include <filesystem>
#include <string_view>
#include <vector>

export module storage_server:fs_ops;

import :types;
import network;

namespace storage {

export class FileSystem {
public:
  [[nodiscard]] static std::expected<void, Error>
  create_file(std::string_view path);
  [[nodiscard]] static std::expected<void, Error>
  create_directory(std::string_view path);
  [[nodiscard]] static std::expected<void, Error>
  delete_file(std::string_view path);
  [[nodiscard]] static std::expected<void, Error>
  delete_directory(std::string_view path);

  [[nodiscard]] static std::vector<std::string>
  list_contents(std::string_view path);
  [[nodiscard]] static bool exists(std::string_view path) noexcept;

  [[nodiscard]] static std::expected<void, Error>
  read_file(std::string_view path, const network::Socket &client_sock);
  [[nodiscard]] static std::expected<void, Error>
  write_file(std::string_view path, const network::Socket &client_sock);
};

} // namespace storage
