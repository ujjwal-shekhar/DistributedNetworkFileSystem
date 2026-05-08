module;

#include <expected>
#include <memory>
#include <string_view>

export module storage_server;

export import :types;
export import :fs_ops;
export import :sync;

namespace storage {

export class StorageServer {
public:
  explicit StorageServer(Config config);
  ~StorageServer();

  StorageServer(const StorageServer &) = delete;
  StorageServer &operator=(const StorageServer &) = delete;

  [[nodiscard]] std::expected<void, Error> start();
  void stop();

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace storage
