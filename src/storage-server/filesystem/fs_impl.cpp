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
import logger;

namespace storage {

namespace fs = std::filesystem;

std::expected<void, Error> FileSystem::create_file(std::string_view path) {
  try {
    fs::path p{std::string(path)};
    if (p.has_parent_path()) {
      std::error_code ec;
      fs::create_directories(p.parent_path(), ec);
    }

    if (fs::exists(path)) {
      logger::warn("FileSystem: File already exists: " + std::string(path));
      return std::unexpected(Error::AlreadyExists);
    }
    std::ofstream file{std::string(path)};
    if (!file) {
      logger::error("FileSystem: Failed to open file for writing: " + std::string(path));
      return std::unexpected(Error::OperationFailed);
    }
    return {};
  } catch (const std::exception &e) {
    logger::error("FileSystem: Exception in create_file: " + std::string(e.what()));
    return std::unexpected(Error::OperationFailed);
  } catch (...) {
    logger::error("FileSystem: Unknown exception in create_file");
    return std::unexpected(Error::OperationFailed);
  }
}

std::expected<void, Error> FileSystem::create_directory(std::string_view path) {
  std::error_code ec;
  if (fs::exists(path)) {
    logger::warn("FileSystem: Directory already exists: " + std::string(path));
    return {}; // Consistent with mkdir -p
  }
  if (fs::create_directories(path, ec))
    return {};
  if (ec) {
    logger::error("FileSystem: Failed to create directory " + std::string(path) + ": " + ec.message());
    return std::unexpected(Error::OperationFailed);
  }
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
  while (file.read(packet.chunk, sizeof(packet.chunk))) {
    packet.size = file.gcount();
    packet.is_last = file.peek() == EOF;
    (void)network::send_all(client_sock, &packet, sizeof(packet));
    if (packet.is_last)
      return {};
  }

  // Handle last partial chunk
  if (file.gcount() > 0 || file.eof()) {
    packet.size = file.gcount();
    packet.is_last = true;
    (void)network::send_all(client_sock, &packet, sizeof(packet));
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
    auto recv_res = network::receive_all(client_sock, &packet, sizeof(packet));
    if (!recv_res)
      break;

    if (packet.size > 0) {
      file.write(packet.chunk, packet.size);
    }
    
    if (packet.is_last)
      break;
  }

  return {};
}

} // namespace storage
