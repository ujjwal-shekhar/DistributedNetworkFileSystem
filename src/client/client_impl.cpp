module;

#include <cstring>
#include <expected>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

module client;

import network;
import naming_server;
import logger;
import commands;

namespace client {

struct Client::Impl {
  std::optional<network::Socket> ns_socket;
  Session session;

  std::expected<void, Error> process_command(std::string_view line) {
    std::stringstream ss(std::string{line});
    std::string cmd_str;
    ss >> cmd_str;

    auto cmd_opt = commands::string_to_command(cmd_str);
    if (!cmd_opt)
      return std::unexpected(Error::InvalidCommand);

    commands::Command cmd = *cmd_opt;
    auto meta = commands::get_metadata(cmd);

    commands::ClientRequest request{.command = cmd};
    std::string arg;
    if (ss >> arg) {
      std::strncpy(request.arg1, arg.c_str(), sizeof(request.arg1));
    }
    if (ss >> arg) {
      std::strncpy(request.arg2, arg.c_str(), sizeof(request.arg2));
    }

    switch (meta.tier) {
    case commands::PrivilegeTier::USER: {
      auto res = handle_user_command(request);
      if (res)
        std::cout << "Command Success.\n";
      else
        std::cout << "Command Failed.\n";
      return res;
    }
    case commands::PrivilegeTier::PRIVILEGED: {
      auto res = handle_privileged_command(request);
      if (res)
        std::cout << "Command Success.\n";
      else
        std::cout << "Command Failed.\n";
      return res;
    }
    case commands::PrivilegeTier::ADMIN: {
      auto res = handle_admin_command(request);
      if (res)
        std::cout << "Command Success.\n";
      else
        std::cout << "Command Failed.\n";
      return res;
    }
    }
    return {};
  }

  std::expected<void, Error>
  handle_user_command(const commands::ClientRequest &request) {
    if (!ns_socket)
      return std::unexpected(Error::ConnectionFailed);
    (void)ns_socket->send(&request, sizeof(request));

    commands::AckPacket ack;
    auto recv_ack_res = ns_socket->receive(&ack, sizeof(ack));
    if (!recv_ack_res || ack.status != commands::Status::Success)
      return std::unexpected(Error::InvalidCommand);

    if (request.command == commands::Command::LIST_ALL) {
      for (int i = 0; i < ack.extra_count; ++i) {
        char buf[commands::MAX_PATH_LEN]{};
        (void)ns_socket->receive(buf, sizeof(buf));
        std::cout << " - " << buf << "\n";
      }
      return {};
    }

    commands::ServerDetails ss_details;
    auto recv_ss_res = ns_socket->receive(&ss_details, sizeof(ss_details));
    if (!recv_ss_res || *recv_ss_res < sizeof(ss_details))
      return std::unexpected(Error::ConnectionFailed);

    auto ss_sock_res =
        network::connect_to_server(ss_details.ip, ss_details.port_client);
    if (!ss_sock_res)
      return std::unexpected(Error::ConnectionFailed);
    auto &ss_sock = *ss_sock_res;

    (void)ss_sock.send(&request, sizeof(request));

    if (request.command == commands::Command::READ_FILE) {
      return receive_file_stream(ss_sock);
    }
    return {};
  }

  std::expected<void, Error>
  receive_file_stream(const network::Socket &ss_sock) {
    commands::FilePacket packet;
    while (true) {
      auto recv_res = ss_sock.receive(&packet, sizeof(packet));
      if (!recv_res || *recv_res < sizeof(packet))
        break;
      std::cout << packet.chunk << std::flush;
      if (packet.is_last)
        break;
    }
    std::cout << std::endl;
    return {};
  }

  std::expected<void, Error>
  handle_privileged_command(const commands::ClientRequest &request) {
    if (!ns_socket)
      return std::unexpected(Error::ConnectionFailed);
    (void)ns_socket->send(&request, sizeof(request));
    commands::AckPacket ack;
    auto recv_res = ns_socket->receive(&ack, sizeof(ack));
    if (!recv_res || ack.status != commands::Status::Success)
      return std::unexpected(Error::InvalidCommand);
    return {};
  }

  std::expected<void, Error>
  handle_admin_command(const commands::ClientRequest &request) {
    if (!session.is_admin) {
      std::string password;
      std::cout << "Enter Admin Password: ";
      if (!(std::cin >> password))
        return std::unexpected(Error::AuthenticationRequired);
      if (naming::AdminService::authenticate(password))
        session.is_admin = true;
      else
        return std::unexpected(Error::AuthenticationRequired);
    }
    return handle_privileged_command(request);
  }
};

Client::Client() : impl_(std::make_unique<Impl>()) {}
Client::~Client() = default;

std::expected<void, Error> Client::connect(std::string_view ip, int port) {
  auto sock_res = network::connect_to_server(std::string(ip), port);
  if (!sock_res)
    return std::unexpected(Error::ConnectionFailed);
  impl_->ns_socket = std::move(*sock_res);
  return {};
}

std::expected<void, Error> Client::run_interactive_loop() {
  std::string line;
  while (true) {
    std::cout << "DFS> " << std::flush;
    if (!std::getline(std::cin, line) || line == "exit")
      break;
    auto res = impl_->process_command(line);
    if (!res)
      logger::error("Command failed.");
  }
  return {};
}

} // namespace client
