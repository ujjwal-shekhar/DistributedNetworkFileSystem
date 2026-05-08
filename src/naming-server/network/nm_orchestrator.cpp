module;

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <expected>
#include <list>
#include <memory>
#include <mutex>
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

  explicit Impl(commands::Config cfg) : config(std::move(cfg)) {}

  void ss_connection_handler(network::Socket ss_sock, std::stop_token st) {
    commands::ServerDetails details;
    auto recv_res = ss_sock.receive(&details, sizeof(details));
    if (!recv_res || *recv_res != sizeof(details))
      return;

    int assigned_id = service.register_server(details);
    logger::info("Registered Storage Server ID: " +
                 std::to_string(assigned_id));

    (void)ss_sock.send(&assigned_id, sizeof(assigned_id));

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

    {
      std::lock_guard lock(registration_mutex);
      // If we are still in the initial wait phase, we don't notify here,
      // but the count_online_servers will reflect the change for anyone
      // waiting.
    }
  }

  void storage_server_listener(std::stop_token st) {
    logger::info("Naming Server: Listening for Storage Servers on port " +
                 std::to_string(config.nm_ss_reg_port));

    auto listen_sock_res = network::create_server_socket(config.nm_ss_reg_port);
    if (!listen_sock_res)
      return;

    auto &listen_sock = *listen_sock_res;
    while (!st.stop_requested()) {
      auto ss_sock_res = listen_sock.accept();
      if (!ss_sock_res)
        continue;

      // Spawn a handler for this SS connection
      ss_handlers.emplace_back(
          [this](std::stop_token st_inner, network::Socket sock) {
            ss_connection_handler(std::move(sock), st_inner);
          },
          std::move(*ss_sock_res));

      // Cleanup finished handlers occasionally
      if (ss_handlers.size() > 100) {
        ss_handlers.remove_if(
            [](const std::jthread &t) { return !t.joinable(); });
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

    logger::info("Naming Server: All required Storage Servers connected. Now "
                 "listening for Clients on port " +
                 std::to_string(config.nm_clt_port));

    auto listen_sock_res = network::create_server_socket(config.nm_clt_port);
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

      auto cmd_opt = commands::string_to_command(request.arg1);

      int ss_id_res = -1;
      auto find_res = service.find_storage_server(request.arg1);
      if (find_res)
        ss_id_res = *find_res;

      if (ss_id_res != -1) {
        auto ss_details = service.get_server_details(ss_id_res);
        if (ss_details) {
          commands::AckPacket ack{.status = commands::Status::Success};
          (void)client_sock.send(&ack, sizeof(ack));
          (void)client_sock.send(&(*ss_details), sizeof(*ss_details));
        }
      } else {
        commands::AckPacket ack;
        ack.status = commands::Status::Error;
        (void)client_sock.send(&ack, sizeof(ack));
      }
    }
  }
};

NamingServer::NamingServer(commands::Config config)
    : impl_(std::make_unique<Impl>(std::move(config))) {}
NamingServer::~NamingServer() { stop(); }

std::expected<void, Error> NamingServer::start(int min_ss) {
  if (impl_->running)
    return {};
  impl_->running = true;
  impl_->min_required_ss = min_ss;
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
