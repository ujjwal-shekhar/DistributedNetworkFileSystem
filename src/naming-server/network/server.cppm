module;

#include <expected>
#include <memory>
import commands;

export module naming_server:server;

import :types;
import core;

namespace naming {

export class NamingServer {
public:
  explicit NamingServer(commands::Config config);
  ~NamingServer();

  NamingServer(const NamingServer &) = delete;
  NamingServer &operator=(const NamingServer &) = delete;

  [[nodiscard]] std::expected<void, Error> start(int min_ss = 3);
  void stop();

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace naming
