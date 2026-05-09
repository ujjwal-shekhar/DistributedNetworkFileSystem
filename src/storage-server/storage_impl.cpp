module;

#include <atomic>
#include <chrono>
#include <cstring>
#include <expected>
#include <filesystem>
#include <memory>
#include <thread>
#include <vector>

module storage_server;

import network;
import logger;
import commands;

namespace storage {

struct StorageServer::Impl {
  Config config;
  LockManager locks;
  std::atomic<bool> running{false};
  std::vector<std::jthread> threads;
  int assigned_id = -1;
  std::optional<network::Socket> registration_sock;

  explicit Impl(Config cfg) : config(std::move(cfg)) {}

  void nm_listener_task(std::stop_token st, int port) {
    logger::info("Storage Server: Listening for NM instructions on port " +
                 std::to_string(port));

    auto listen_sock_res = network::create_server_socket(port);
    if (!listen_sock_res) {
      logger::error("Storage Server: NM listener failed to bind to port " +
                    std::to_string(port));
      return;
    }

    auto &listen_sock = *listen_sock_res;

    while (!st.stop_requested()) {
      auto accept_res = listen_sock.accept();
      if (!accept_res)
        continue;

      auto& [client_sock, peer_ip] = *accept_res;
      commands::ClientRequest request;
      auto recv_res =
          network::receive_all(client_sock, &request, sizeof(request));
      if (!recv_res) {
        logger::error("Storage Server: Failed to receive NM command.");
        continue;
      }

      logger::info("Storage Server: Received command " + 
                   std::string(commands::get_metadata(request.command).name) + 
                   " for path: " + std::string(request.arg1));

      std::string path = request.arg1;
      if (!path.empty() && path[0] == '/')
        path.erase(0, 1);

      commands::AckPacket ack{.status = commands::Status::Success};

      locks.acquire_write(path);

      if (request.command == commands::Command::CREATE_DIR) {
        if (auto res = FileSystem::create_directory(path); !res) {
          ack.status = commands::Status::Error;
          ack.error_code = static_cast<int>(res.error());
          logger::error("Failed to create directory " + path + ": " + std::to_string(ack.error_code));
        }
      } else if (request.command == commands::Command::CREATE_FILE) {
        if (auto res = FileSystem::create_file(path); !res) {
          ack.status = commands::Status::Error;
          ack.error_code = static_cast<int>(res.error());
          logger::error("Failed to create file " + path + ": " + std::to_string(ack.error_code));
        }
      } else if (request.command == commands::Command::DELETE_FILE) {
        if (auto res = FileSystem::delete_file(path); !res) {
          ack.status = commands::Status::Error;
          ack.error_code = static_cast<int>(res.error());
          logger::error("Failed to delete file " + path + ": " + std::to_string(ack.error_code));
        }
      } else if (request.command == commands::Command::DELETE_DIR) {
        if (auto res = FileSystem::delete_directory(path); !res) {
          ack.status = commands::Status::Error;
          ack.error_code = static_cast<int>(res.error());
          logger::error("Failed to delete directory " + path + ": " + std::to_string(ack.error_code));
        }
      } else if (request.command == commands::Command::REPLICATE_FILE) {
        std::string target_info = request.arg2;
        auto pos = target_info.find(':');
        if (pos != std::string::npos) {
          std::string host = target_info.substr(0, pos);
          int port = std::stoi(target_info.substr(pos + 1));
          if (!replicate_to_peer(path, host, port)) {
            ack.status = commands::Status::Error;
            logger::error("Failed to replicate " + path + " to " + target_info);
          }
        } else {
          ack.status = commands::Status::Error;
        }
      }

      locks.release_write(path);
      (void)network::send_all(client_sock, &ack, sizeof(ack));
    }
  }

  bool replicate_to_peer(std::string_view path, const std::string& host, int port) {
    logger::info("Storage Server: Replicating " + std::string(path) + " to " + host + ":" + std::to_string(port));
    
    auto peer_sock_res = network::connect_to_server(host, port);
    if (!peer_sock_res) return false;
    auto& peer_sock = *peer_sock_res;

    commands::ClientRequest push_req{};
    push_req.command = commands::Command::WRITE_FILE;
    std::strncpy(push_req.arg1, std::string(path).c_str(), sizeof(push_req.arg1) - 1);
    
    (void)network::send_all(peer_sock, &push_req, sizeof(push_req));
    
    auto res = FileSystem::read_file(path, peer_sock);
    if (!res) return false;

    commands::AckPacket ack;
    auto ack_res = network::receive_all(peer_sock, &ack, sizeof(ack));
    return ack_res && ack.status == commands::Status::Success;
  }

  void client_listener_task(std::stop_token st, int port) {
    logger::info("Storage Server: Listening for Client requests on port " +
                 std::to_string(port));

    auto listen_sock_res = network::create_server_socket(port);
    if (!listen_sock_res) {
      logger::error("Storage Server: Client listener failed to bind to port " +
                    std::to_string(port));
      return;
    }

    auto &listen_sock = *listen_sock_res;

    while (!st.stop_requested()) {
      auto accept_res = listen_sock.accept();
      if (!accept_res)
        continue;

      auto& [client_sock, peer_ip] = *accept_res;
      commands::ClientRequest request;
      auto recv_res =
          network::receive_all(client_sock, &request, sizeof(request));
      if (!recv_res) {
        logger::error("Storage Server: Failed to receive Client command.");
        continue;
      }

      logger::info("Storage Server: Received command " + 
                   std::string(commands::get_metadata(request.command).name) + 
                   " for path: " + std::string(request.arg1));

      std::string path = request.arg1;
      if (!path.empty() && path[0] == '/')
        path.erase(0, 1);

      commands::AckPacket ack{.status = commands::Status::Success};

      if (request.command == commands::Command::READ_FILE) {
        locks.acquire_read(path);
        if (auto res = FileSystem::read_file(path, client_sock); !res) {
          ack.status = commands::Status::Error;
          ack.error_code = static_cast<int>(res.error());
          logger::error("Failed to read file " + path + ": " + std::to_string(ack.error_code));
        }
        locks.release_read(path);
      } else if (request.command == commands::Command::WRITE_FILE) {
        locks.acquire_write(path);
        if (auto res = FileSystem::write_file(path, client_sock); !res) {
          ack.status = commands::Status::Error;
          ack.error_code = static_cast<int>(res.error());
          logger::error("Failed to write file " + path + ": " + std::to_string(ack.error_code));
        }
        locks.release_write(path);
      }

      (void)network::send_all(client_sock, &ack, sizeof(ack));
    }
  }

  void registration_task(int nm_port, int client_port) {
    logger::info("Storage Server: Attempting to register with Naming Server at " +
                 config.nm_ip + ":5049");

    int retries = 0;
    const int max_retries = 10;
    
    while (retries < max_retries) {
      auto sock_res = network::connect_to_server(config.nm_ip, 5049);
      if (sock_res) {
        registration_sock = std::move(*sock_res);
        break;
      }
      
      retries++;
      logger::warn("Storage Server: Failed to connect to Naming Server (attempt " + 
                   std::to_string(retries) + "/" + std::to_string(max_retries) + "). Retrying in 2s...");
      std::this_thread::sleep_for(std::chrono::seconds(2));
    }

    if (!registration_sock) {
      logger::error("Storage Server: Exhausted registration retries. Exiting.");
      return;
    }

    auto &sock = *registration_sock;

    commands::ServerDetails details{
        .id = -1,
        .port_nm = nm_port,
        .port_client = client_port,
        .path_count = static_cast<int>(config.accessible_paths.size()),
        .online = true};
    std::strncpy(details.ip, "127.0.0.1", sizeof(details.ip));
    for (int i = 0; i < details.path_count && i < 10; ++i) {
      std::strncpy(details.paths[i], config.accessible_paths[i].c_str(), 256);
    }

    auto send_res = network::send_all(sock, &details, sizeof(details));
    if (!send_res) {
      logger::error("Storage Server: Failed to send registration details.");
      return;
    }

    int id = -1;
    auto recv_res = network::receive_all(sock, &id, sizeof(id));
    if (recv_res) {
      assigned_id = id;
      logger::info("Storage Server: Registered with ID: " +
                   std::to_string(assigned_id));
    } else {
      logger::error(
          "Storage Server: Failed to receive assigned ID from Naming Server.");
    }
  }
};

StorageServer::StorageServer(Config config)
    : impl_(std::make_unique<Impl>(std::move(config))) {}
StorageServer::~StorageServer() { stop(); }

std::expected<void, Error> StorageServer::start() {
  int nm_port = 0;
  int client_port = 0;

  {
    auto nm_sock_res = network::create_server_socket(0);
    if (!nm_sock_res)
      return std::unexpected(Error::NetworkError);
    nm_port = nm_sock_res->get_port().value_or(0);

    auto client_sock_res = network::create_server_socket(0);
    if (!client_sock_res)
      return std::unexpected(Error::NetworkError);
    client_port = client_sock_res->get_port().value_or(0);

    logger::info("Storage Server: Ephemeral ports selected: NM=" +
                 std::to_string(nm_port) +
                 " Client=" + std::to_string(client_port));
  }

  impl_->running = true;
  impl_->registration_task(nm_port, client_port);

  if (impl_->assigned_id == -1) {
    logger::error("Storage Server: Registration failed, cannot start listeners.");
    return std::unexpected(Error::NetworkError);
  }

  std::string root = impl_->config.storage_root;
  if (root.empty()) {
    root = "mnt/SS_" + std::to_string(impl_->assigned_id);
  }

  std::error_code ec;
  std::filesystem::create_directories(root, ec);
  std::filesystem::current_path(root, ec);
  if (ec) {
    logger::error("Failed to set storage root " + root + ": " + ec.message());
    return std::unexpected(Error::OperationFailed);
  }
  logger::info("Storage Server: Storage root isolated at: " + root);

  impl_->threads.emplace_back([this, nm_port](std::stop_token st) {
    impl_->nm_listener_task(st, nm_port);
  });
  impl_->threads.emplace_back([this, client_port](std::stop_token st) {
    impl_->client_listener_task(st, client_port);
  });
  return {};
}

void StorageServer::stop() {
  impl_->running = false;
  impl_->threads.clear();
}

} // namespace storage
