module;

#include <expected>
#include <memory>
#include <string_view>

export module client;

export import :types;

export namespace client {

class Client {
public:
  Client();
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
