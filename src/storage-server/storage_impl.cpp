module;

#include <atomic>
#include <chrono>
#include <cstring>
#include <expected>
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
    if (!listen_sock_res)
      return;

    auto &listen_sock = *listen_sock_res;

    while (!st.stop_requested()) {
      auto client_sock_res = listen_sock.accept();
      if (!client_sock_res)
        continue;

      auto &client_sock = *client_sock_res;
      commands::ClientRequest request;
      auto recv_res = client_sock.receive(&request, sizeof(request));
      if (!recv_res || *recv_res < sizeof(request))
        continue;

      std::string path = request.arg1;
      if (!path.empty() && path[0] == '/')
        path.erase(0, 1);

      commands::AckPacket ack{.status = commands::Status::Success};

      locks.acquire_write(path);

      if (request.command == commands::Command::CREATE_DIR) {
        if (!FileSystem::create_directory(path))
          ack.status = commands::Status::Error;
      } else if (request.command == commands::Command::CREATE_FILE) {
        if (!FileSystem::create_file(path))
          ack.status = commands::Status::Error;
      } else if (request.command == commands::Command::DELETE_FILE) {
        if (!FileSystem::delete_file(path))
          ack.status = commands::Status::Error;
      }

      locks.release_write(path);
      (void)client_sock.send(&ack, sizeof(ack));
    }
  }

  void client_listener_task(std::stop_token st, int port) {
    logger::info("Storage Server: Listening for Client requests on port " +
                 std::to_string(port));

    auto listen_sock_res = network::create_server_socket(port);
    if (!listen_sock_res)
      return;

    auto &listen_sock = *listen_sock_res;

    while (!st.stop_requested()) {
      auto client_sock_res = listen_sock.accept();
      if (!client_sock_res)
        continue;

      auto &client_sock = *client_sock_res;
      commands::ClientRequest request;
      auto recv_res = client_sock.receive(&request, sizeof(request));
      if (!recv_res || *recv_res < sizeof(request))
        continue;

      std::string path = request.arg1;
      if (!path.empty() && path[0] == '/')
        path.erase(0, 1);

      commands::AckPacket ack{.status = commands::Status::Success};

      if (request.command == commands::Command::READ_FILE) {
        locks.acquire_read(path);
        if (!FileSystem::read_file(path, client_sock))
          ack.status = commands::Status::Error;
        locks.release_read(path);
      } else if (request.command == commands::Command::WRITE_FILE) {
        locks.acquire_write(path);
        if (!FileSystem::write_file(path, client_sock))
          ack.status = commands::Status::Error;
        locks.release_write(path);
      }

      (void)client_sock.send(&ack, sizeof(ack));
    }
  }

  void registration_task(int nm_port, int client_port) {
    logger::info("Storage Server: Registering with Naming Server at " +
                 config.nm_ip + ":" + std::to_string(config.nm_port));

    auto sock_res = network::connect_to_server(config.nm_ip, 5049);
    if (!sock_res) {
      logger::error("Storage Server: Failed to connect to Naming Server for "
                    "registration.");
      return;
    }

    registration_sock = std::move(*sock_res);
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

    (void)sock.send(&details, sizeof(details));

    int id = -1;
    auto recv_res = sock.receive(&id, sizeof(id));
    if (recv_res && *recv_res == sizeof(id)) {
      assigned_id = id;
      logger::info("Storage Server: Registered with ID: " +
                   std::to_string(assigned_id));
    } else {
      logger::error(
          "Storage Server: Failed to receive assigned ID from Naming Server.");
    }

    // Keep the registration_sock alive to maintain the connection
  }
};

StorageServer::StorageServer(Config config)
    : impl_(std::make_unique<Impl>(std::move(config))) {}
StorageServer::~StorageServer() { stop(); }

std::expected<void, Error> StorageServer::start() {
  impl_->running = true;

  auto nm_sock_res = network::create_server_socket(0);
  if (!nm_sock_res)
    return std::unexpected(Error::NetworkError);
  int nm_port = nm_sock_res->get_port().value_or(0);

  auto client_sock_res = network::create_server_socket(0);
  if (!client_sock_res)
    return std::unexpected(Error::NetworkError);
  int client_port = client_sock_res->get_port().value_or(0);

  impl_->registration_task(nm_port, client_port);

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
