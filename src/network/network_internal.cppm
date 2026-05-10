module;

#include <arpa/inet.h>
#include <expected>
#include <netdb.h>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

export module network:internal;

import :types;

namespace network {

Socket::~Socket() {
  if (is_valid())
    close(fd_);
}

Socket &Socket::operator=(Socket &&other) noexcept {
  if (this != &other) {
    if (is_valid())
      close(fd_);
    fd_ = other.fd_;
    other.fd_ = Socket::INVALID;
  }
  return *this;
}

std::expected<size_t, Error> Socket::send(const void *data,
                                          size_t size) const noexcept {
  if (!is_valid())
    return std::unexpected(Error::SendFailed);
  ssize_t bytes = ::send(fd_, data, size, 0);
  if (bytes < 0)
    return std::unexpected(Error::SendFailed);
  return static_cast<size_t>(bytes);
}

std::expected<size_t, Error> Socket::receive(void *buffer,
                                             size_t size) const noexcept {
  if (!is_valid())
    return std::unexpected(Error::ReceiveFailed);
  ssize_t bytes = ::recv(fd_, buffer, size, 0);
  if (bytes < 0)
    return std::unexpected(Error::ReceiveFailed);
  return static_cast<size_t>(bytes);
}

std::expected<std::pair<Socket, std::string>, Error>
Socket::accept() const noexcept {
  if (!is_valid())
    return std::unexpected(Error::AcceptFailed);

  sockaddr_in client_addr{};
  socklen_t client_len = sizeof(client_addr);
  int client_fd = ::accept(fd_, (struct sockaddr *)&client_addr, &client_len);

  if (client_fd <= Socket::INVALID)
    return std::unexpected(Error::AcceptFailed);

  char ip[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &client_addr.sin_addr, ip, INET_ADDRSTRLEN);

  return std::make_pair(Socket(client_fd), std::string(ip));
}

std::expected<int, Error> Socket::get_port() const noexcept {
  if (!is_valid())
    return std::unexpected(Error::CreationFailed);
  sockaddr_in addr{};
  socklen_t len = sizeof(addr);
  if (getsockname(fd_, (struct sockaddr *)&addr, &len) == -1) {
    return std::unexpected(Error::CreationFailed);
  }
  return ntohs(addr.sin_port);
}

export std::expected<Socket, Error> create_server_socket(int port) {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd <= Socket::INVALID)
    return std::unexpected(Error::CreationFailed);

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = INADDR_ANY;

  if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
    close(fd);
    return std::unexpected(Error::BindFailed);
  }

  if (listen(fd, 10) == -1) {
    close(fd);
    return std::unexpected(Error::ListenFailed);
  }

  return Socket(fd);
}

export std::expected<Socket, Error> connect_to_server(const std::string &host,
                                                      int port) {
  struct addrinfo hints {
  }, *res;
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  std::string port_str = std::to_string(port);
  if (getaddrinfo(host.c_str(), port_str.c_str(), &hints, &res) != 0) {
    return std::unexpected(Error::ConnectFailed);
  }

  int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (fd <= Socket::INVALID) {
    freeaddrinfo(res);
    return std::unexpected(Error::CreationFailed);
  }

  if (connect(fd, res->ai_addr, res->ai_addrlen) == -1) {
    close(fd);
    freeaddrinfo(res);
    return std::unexpected(Error::ConnectFailed);
  }

  freeaddrinfo(res);
  return Socket(fd);
}

export std::expected<void, Error> send_all(const Socket &sock, const void *data,
                                           size_t size) {
  const uint8_t *ptr = static_cast<const uint8_t *>(data);
  size_t remaining = size;
  while (remaining > 0) {
    auto res = sock.send(ptr, remaining);
    if (!res)
      return std::unexpected(res.error());
    if (*res == 0)
      return std::unexpected(Error::SendFailed);
    ptr += *res;
    remaining -= *res;
  }
  return {};
}

export std::expected<void, Error> receive_all(const Socket &sock, void *buffer,
                                              size_t size) {
  uint8_t *ptr = static_cast<uint8_t *>(buffer);
  size_t remaining = size;
  while (remaining > 0) {
    auto res = sock.receive(ptr, remaining);
    if (!res)
      return std::unexpected(res.error());
    if (*res == 0)
      return std::unexpected(Error::ReceiveFailed);
    ptr += *res;
    remaining -= *res;
  }
  return {};
}

} // namespace network
