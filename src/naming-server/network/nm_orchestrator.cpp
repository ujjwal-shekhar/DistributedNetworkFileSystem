module;

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <expected>
#include <list>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>

module naming_server;

import network;
import logger;
import commands;

namespace naming {

struct NamingServer::Impl {
  NamingService service;
  commands::Config config;
  std::atomic<bool> running{false};
  std::vector<std::jthread> listeners;
  std::list<std::jthread> ss_handlers;

  std::mutex registration_mutex;
  std::condition_variable registration_cv;
  int min_required_ss = 3;
  int replication_factor = 3;

  explicit Impl(commands::Config cfg) : config(std::move(cfg)) {}

  std::expected<void, Error>
  send_to_ss(int ss_id, const commands::ClientRequest &request) {
    auto details = service.get_server_details(ss_id);
    if (!details) {
      logger::error("Naming Server: Cannot find details for SS ID: " +
                    std::to_string(ss_id));
      return std::unexpected(Error::OperationFailed);
    }

    auto ss_sock_res =
        network::connect_to_server(details->ip, details->port_nm);
    if (!ss_sock_res) {
      logger::error("Naming Server: Failed to connect to SS ID " +
                    std::to_string(ss_id) + " at " + details->ip + ":" +
                    std::to_string(details->port_nm));
      return std::unexpected(Error::NetworkError);
    }

    (void)network::send_all(*ss_sock_res, &request, sizeof(request));

    commands::AckPacket ack;
    auto recv_res = network::receive_all(*ss_sock_res, &ack, sizeof(ack));
    if (!recv_res) {
      logger::error("Naming Server: No response from SS ID " +
                    std::to_string(ss_id));
      return std::unexpected(Error::OperationFailed);
    }

    if (ack.status != commands::Status::Success) {
      logger::error("Naming Server: SS ID " + std::to_string(ss_id) +
                    " returned error code: " + std::to_string(ack.error_code));
      return std::unexpected(Error::OperationFailed);
    }

    return {};
  }

  void ss_connection_handler(network::Socket ss_sock, std::stop_token st) {
    commands::ServerDetails details;
    auto recv_res = network::receive_all(ss_sock, &details, sizeof(details));
    if (!recv_res) {
      logger::error(
          "Naming Server: Failed to receive registration details from "
          "Storage Server.");
      return;
    }

    int assigned_id = service.register_server(details);
    logger::info("Registered Storage Server ID: " +
                 std::to_string(assigned_id));

    (void)network::send_all(ss_sock, &assigned_id, sizeof(assigned_id));

    {
      std::lock_guard lock(registration_mutex);
      if (service.count_online_servers() >=
          static_cast<size_t>(min_required_ss)) {
        registration_cv.notify_all();
      }
    }

    // Monitor for disconnection (Keepalive)
    char dummy;
    while (!st.stop_requested()) {
      auto r = ss_sock.receive(&dummy, 1);
      if (r && *r == 0)
        break; // Clean close
      if (!r)
        break; // Error or closed
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    service.mark_server_offline(assigned_id);
    logger::warn("Storage Server ID " + std::to_string(assigned_id) +
                 " went offline.");
  }

  void storage_server_listener(std::stop_token st) {
    logger::info("Naming Server: Listening for Storage Servers on port " +
                 std::to_string(config.nm_ss_reg_port));

    auto listen_sock_res = network::create_server_socket(config.nm_ss_reg_port);
    if (!listen_sock_res) {
      logger::error("Naming Server: Failed to bind registration port " +
                    std::to_string(config.nm_ss_reg_port));
      return;
    }

    auto &listen_sock = *listen_sock_res;
    while (!st.stop_requested()) {
      auto ss_sock_res = listen_sock.accept();
      if (!ss_sock_res)
        continue;

      logger::info("Naming Server: Accepted connection from Storage Server.");

      ss_handlers.emplace_back(
          [this](std::stop_token st_inner, network::Socket sock) {
            ss_connection_handler(std::move(sock), st_inner);
          },
          std::move(*ss_sock_res));

      if (ss_handlers.size() > 100) {
        ss_handlers.remove_if(
            [](const std::jthread &t) { return !t.joinable(); });
      }
    }
  }

  void client_handler(network::Socket client_sock, std::stop_token st) {
    while (!st.stop_requested()) {
      commands::ClientRequest request;
      auto recv_res =
          network::receive_all(client_sock, &request, sizeof(request));
      if (!recv_res) {
        // Connection closed or error
        break;
      }

      if (request.command == commands::Command::LIST_ALL) {
        auto files = service.list_all();
        commands::AckPacket ack{.status = commands::Status::Success};
        ack.extra_count = static_cast<int>(files.size());
        (void)network::send_all(client_sock, &ack, sizeof(ack));

        for (const auto &file : files) {
          char buf[commands::MAX_PATH_LEN]{};
          std::strncpy(buf, file.c_str(), sizeof(buf));
          (void)network::send_all(client_sock, buf, sizeof(buf));
        }
      } else if (request.command == commands::Command::CREATE_FILE ||
                 request.command == commands::Command::CREATE_DIR) {
        if (auto find_res = service.find_storage_server(request.arg1); find_res) {
          logger::warn("Naming Server: Path already exists: " + std::string(request.arg1));
          commands::AckPacket ack{.status = commands::Status::Error, .error_code = 409};
          (void)network::send_all(client_sock, &ack, sizeof(ack));
          continue;
        }

        auto online_ids = service.get_online_server_ids();
        int factor =
            std::min(static_cast<int>(online_ids.size()), replication_factor);

        if (factor == 0) {
          commands::AckPacket ack{.status = commands::Status::Error};
          (void)network::send_all(client_sock, &ack, sizeof(ack));
          continue;
        }

        std::shuffle(online_ids.begin(), online_ids.end(),
                     std::mt19937{std::random_device{}()});
        std::vector<int> selected_ids(online_ids.begin(),
                                      online_ids.begin() + factor);

        bool all_success = true;
        for (int id : selected_ids) {
          if (!send_to_ss(id, request)) {
            all_success = false;
            break;
          }
        }

        if (all_success) {
          service.register_path(request.arg1, selected_ids,
                                request.command ==
                                    commands::Command::CREATE_FILE);
          commands::AckPacket ack{.status = commands::Status::Success};
          (void)network::send_all(client_sock, &ack, sizeof(ack));
          logger::info("Created " + std::string(request.arg1) + " on " +
                       std::to_string(factor) + " servers.");
        } else {
          commands::AckPacket ack{.status = commands::Status::Error, .error_code = 1};
          (void)network::send_all(client_sock, &ack, sizeof(ack));
        }
      } else if (request.command == commands::Command::DELETE_FILE ||
                 request.command == commands::Command::DELETE_DIR) {
        auto find_res = service.find_storage_server(request.arg1);
        if (find_res && !find_res->empty()) {
          bool any_success = false;
          for (int id : *find_res) {
            if (send_to_ss(id, request)) {
              any_success = true;
            }
          }
          if (any_success) {
            if (request.command == commands::Command::DELETE_DIR) {
              // For directories, we need to remove all descendants from the trie
              auto all_files = service.list_all();
              std::string prefix = std::string(request.arg1);
              if (!prefix.empty() && prefix.back() != '/') {
                prefix += "/";
              }
              for (const auto& f : all_files) {
                if (f.starts_with(prefix)) {
                  service.remove_path(f);
                }
              }
            }
            service.remove_path(request.arg1);
            commands::AckPacket ack{.status = commands::Status::Success};
            (void)network::send_all(client_sock, &ack, sizeof(ack));
          } else {
            commands::AckPacket ack{.status = commands::Status::Error, .error_code = 4};
            (void)network::send_all(client_sock, &ack, sizeof(ack));
          }
        } else {
          commands::AckPacket ack{.status = commands::Status::Error, .error_code = 3};
          (void)network::send_all(client_sock, &ack, sizeof(ack));
        }
      } else {
        auto find_res = service.find_storage_server(request.arg1);
        if (find_res && !find_res->empty()) {
          std::vector<commands::ServerDetails> online_replicas;
          for (int id : *find_res) {
            auto details = service.get_server_details(id);
            if (details && details->online) {
              online_replicas.push_back(*details);
            }
          }

          if (!online_replicas.empty()) {
            if (request.command == commands::Command::WRITE_FILE) {
              commands::AckPacket ack{.status = commands::Status::Success};
              ack.extra_count = static_cast<int>(online_replicas.size());
              (void)network::send_all(client_sock, &ack, sizeof(ack));
              for (const auto &details : online_replicas) {
                (void)network::send_all(client_sock, &details, sizeof(details));
              }
            } else {
              // For READ_FILE and others, just pick one (Load balancing)
              commands::AckPacket ack{.status = commands::Status::Success};
              ack.extra_count = 1;
              (void)network::send_all(client_sock, &ack, sizeof(ack));
              
              std::shuffle(online_replicas.begin(), online_replicas.end(),
                           std::mt19937{std::random_device{}()});
              (void)network::send_all(client_sock, &online_replicas[0],
                                      sizeof(online_replicas[0]));
            }
          } else {
            commands::AckPacket ack{.status = commands::Status::Error, .error_code = 2};
            (void)network::send_all(client_sock, &ack, sizeof(ack));
          }
        } else {
          commands::AckPacket ack{.status = commands::Status::Error, .error_code = 3};
          (void)network::send_all(client_sock, &ack, sizeof(ack));
        }
      }
    }
  }

  void client_listener(std::stop_token st) {
    {
      std::unique_lock lock(registration_mutex);
      while (!st.stop_requested() && service.count_online_servers() <
                                         static_cast<size_t>(min_required_ss)) {
        logger::info("Naming Server: Waiting for " +
                     std::to_string(min_required_ss) +
                     " Storage Servers (Current: " +
                     std::to_string(service.count_online_servers()) + ")");
        registration_cv.wait(lock);
      }
    }

    if (st.stop_requested())
      return;

    logger::info(
        "Naming Server: All required Storage Servers connected. Listening for "
        "Clients on port " +
        std::to_string(config.nm_clt_port));

    auto listen_sock_res = network::create_server_socket(config.nm_clt_port);
    if (!listen_sock_res) {
      logger::error("Naming Server: Failed to bind Client port " +
                    std::to_string(config.nm_clt_port));
      return;
    }

    auto &listen_sock = *listen_sock_res;
    while (!st.stop_requested()) {
      auto client_sock_res = listen_sock.accept();
      if (!client_sock_res)
        continue;

      ss_handlers.emplace_back(
          [this](std::stop_token st_inner, network::Socket sock) {
            client_handler(std::move(sock), st_inner);
          },
          std::move(*client_sock_res));
    }
  }
};

NamingServer::NamingServer(commands::Config config)
    : impl_(std::make_unique<Impl>(std::move(config))) {}
NamingServer::~NamingServer() { stop(); }

std::expected<void, Error> NamingServer::start(int min_ss,
                                               int replication_factor) {
  if (impl_->running)
    return {};
  impl_->running = true;
  impl_->min_required_ss = min_ss;
  impl_->replication_factor = replication_factor;
  impl_->listeners.emplace_back(
      [this](std::stop_token st) { impl_->storage_server_listener(st); });
  impl_->listeners.emplace_back(
      [this](std::stop_token st) { impl_->client_listener(st); });
  return {};
}

void NamingServer::stop() {
  impl_->running = false;
  impl_->listeners.clear();
  impl_->ss_handlers.clear();
}

} // namespace naming
