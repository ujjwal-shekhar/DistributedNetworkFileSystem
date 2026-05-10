module;

#include <expected>
#include <memory>
#include <string_view>

export module client;

export import :types;
export import :utils;
export import :prompt;
export import :warp;
export import :peek;
export import :pastevents;
export import :input;

export namespace client {

class Client {
public:
  explicit Client(size_t history_size = 5);
  ~Client();

  Client(const Client &) = delete;
  Client &operator=(const Client &) = delete;

  [[nodiscard]] std::expected<void, Error> connect(std::string_view ip,
                                                   int port);
  [[nodiscard]] std::expected<void, Error> run_interactive_loop();

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace client
