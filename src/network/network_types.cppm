module;

#include <cstddef>
#include <expected>

export module network:types;

namespace network {

export enum class Error {
  CreationFailed,
  BindFailed,
  ListenFailed,
  AcceptFailed,
  ConnectFailed,
  ReadFailed,
  WriteFailed,
  SendFailed,
  ReceiveFailed
};

export class Socket {
public:
  static constexpr int INVALID = -1;

  explicit Socket(int fd) noexcept : fd_(fd) {}
  ~Socket();

  Socket(const Socket &) = delete;
  Socket &operator=(const Socket &) = delete;

  Socket(Socket &&other) noexcept : fd_(other.fd_) { other.fd_ = Socket::INVALID; }

  Socket &operator=(Socket &&other) noexcept;

  [[nodiscard]] int fd() const noexcept { return fd_; }
  [[nodiscard]] bool is_valid() const noexcept { return fd_ > Socket::INVALID; }
  [[nodiscard]] std::expected<int, Error> get_port() const noexcept;

  [[nodiscard]] std::expected<size_t, Error> send(const void *data,
                                                  size_t size) const noexcept;
  [[nodiscard]] std::expected<size_t, Error>
  receive(void *buffer, size_t size) const noexcept;
  [[nodiscard]] std::expected<Socket, Error> accept() const noexcept;

private:
  int fd_;
};

} // namespace network
