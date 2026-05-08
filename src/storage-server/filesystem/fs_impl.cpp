module;

#include <cstring>
#include <expected>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <system_error>
#include <vector>

module storage_server;

import network;
import commands;

namespace storage {

namespace fs = std::filesystem;

std::expected<void, Error> FileSystem::create_file(std::string_view path) {
  try {
    if (fs::exists(path))
      return std::unexpected(Error::AlreadyExists);
    std::ofstream file{std::string(path)};
    if (!file)
      return std::unexpected(Error::OperationFailed);
    return {};
  } catch (...) {
    return std::unexpected(Error::OperationFailed);
  }
}

std::expected<void, Error> FileSystem::create_directory(std::string_view path) {
  std::error_code ec;
  if (fs::create_directories(path, ec))
    return {};
  if (ec)
    return std::unexpected(Error::OperationFailed);
  return {};
}

std::expected<void, Error> FileSystem::delete_file(std::string_view path) {
  std::error_code ec;
  if (fs::remove(path, ec))
    return {};
  return std::unexpected(Error::FileNotFound);
}

std::expected<void, Error> FileSystem::delete_directory(std::string_view path) {
  std::error_code ec;
  if (fs::remove_all(path, ec) > 0)
    return {};
  return std::unexpected(Error::FileNotFound);
}

std::vector<std::string> FileSystem::list_contents(std::string_view path) {
  std::vector<std::string> results;
  try {
    for (const auto &entry : fs::recursive_directory_iterator(path)) {
      results.push_back(entry.path().string());
    }
  } catch (...) {
  }
  return results;
}

bool FileSystem::exists(std::string_view path) noexcept {
  return fs::exists(path);
}

std::expected<void, Error>
FileSystem::read_file(std::string_view path,
                      const network::Socket &client_sock) {
  std::ifstream file{std::string(path), std::ios::binary};
  if (!file)
    return std::unexpected(Error::FileNotFound);

  commands::FilePacket packet;
  while (file.read(packet.chunk, sizeof(packet.chunk) - 1)) {
    packet.chunk[file.gcount()] = '\0';
    packet.is_last = file.peek() == EOF;
    (void)client_sock.send(&packet, sizeof(packet));
    if (packet.is_last)
      return {};
  }

  // Handle last partial chunk
  if (file.gcount() > 0 || file.eof()) {
    packet.chunk[file.gcount()] = '\0';
    packet.is_last = true;
    (void)client_sock.send(&packet, sizeof(packet));
  }

  return {};
}

std::expected<void, Error>
FileSystem::write_file(std::string_view path,
                       const network::Socket &client_sock) {
  std::ofstream file{std::string(path), std::ios::binary};
  if (!file)
    return std::unexpected(Error::OperationFailed);

  commands::FilePacket packet;
  while (true) {
    auto recv_res = client_sock.receive(&packet, sizeof(packet));
    if (!recv_res || *recv_res < sizeof(packet))
      break;

    file.write(packet.chunk, std::strlen(packet.chunk));
    if (packet.is_last)
      break;
  }

  return {};
}

} // namespace storage
