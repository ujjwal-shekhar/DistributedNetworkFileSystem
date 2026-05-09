module;

#include <cstddef>
#include <cstring>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

export module naming_server;

import commands;

export import :types;
export import :commands;
export import :admin;
export import :client_manager;
export import :server;

namespace naming {

export class NamingService {
public:
  NamingService();
  ~NamingService();

  NamingService(const NamingService &) = delete;
  NamingService &operator=(const NamingService &) = delete;

  [[nodiscard]] std::expected<std::vector<int>, Error>
  find_storage_server(std::string_view path);
  void register_path(std::string_view path, const std::vector<int> &server_ids,
                     bool is_file);
  void remove_path(std::string_view path);

  [[nodiscard]] std::vector<std::string> list_all() const;

  int register_server(const commands::ServerDetails &details);
  void mark_server_offline(int server_id);
  [[nodiscard]] size_t count_online_servers() const noexcept;
  [[nodiscard]] size_t count_registered_servers() const noexcept;
  [[nodiscard]] std::vector<int> get_online_server_ids() const;
  [[nodiscard]] std::optional<commands::ServerDetails>
  get_server_details(int server_id);

  [[nodiscard]] ClientManager &clients() noexcept;

private:
  void register_path_internal(std::string_view path, int server_id,
                              bool is_file);

  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace naming
