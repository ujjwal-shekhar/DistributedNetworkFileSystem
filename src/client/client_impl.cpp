module;

#include <cstring>
#include <expected>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <chrono>
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

    commands::ClientRequest request{};
    request.command = cmd;
    
    std::string arg;
    if (ss >> arg) {
      std::strncpy(request.arg1, arg.c_str(), sizeof(request.arg1) - 1);
    }
    if (ss >> arg) {
      std::strncpy(request.arg2, arg.c_str(), sizeof(request.arg2) - 1);
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
    (void)network::send_all(*ns_socket, &request, sizeof(request));

    commands::AckPacket ack;
    auto recv_ack_res = network::receive_all(*ns_socket, &ack, sizeof(ack));
    if (!recv_ack_res || ack.status != commands::Status::Success) {
      if (recv_ack_res) {
        logger::error("Naming Server returned error code: " +
                      std::to_string(ack.error_code));
      }
      return std::unexpected(Error::InvalidCommand);
    }

    if (request.command == commands::Command::LIST_ALL) {
      for (int i = 0; i < ack.extra_count; ++i) {
        char buf[commands::MAX_PATH_LEN]{};
        (void)network::receive_all(*ns_socket, buf, sizeof(buf));
        std::cout << " - " << buf << "\n";
      }
      return {};
    }

    std::vector<commands::ServerDetails> replicas;
    for (int i = 0; i < ack.extra_count; ++i) {
      commands::ServerDetails ss_details;
      auto recv_ss_res =
          network::receive_all(*ns_socket, &ss_details, sizeof(ss_details));
      if (recv_ss_res) {
        replicas.push_back(ss_details);
      }
    }

    if (replicas.empty())
      return std::unexpected(Error::ConnectionFailed);

    if (request.command == commands::Command::READ_FILE) {
      auto ss_sock_res =
          network::connect_to_server(replicas[0].ip, replicas[0].port_client);
      if (!ss_sock_res)
        return std::unexpected(Error::ConnectionFailed);
      auto &ss_sock = *ss_sock_res;
      (void)network::send_all(ss_sock, &request, sizeof(request));
      return receive_file_stream(ss_sock);
    }

    if (request.command == commands::Command::WRITE_FILE) {
      std::vector<commands::FilePacket> buffer;
      if (request.arg2[0] != '\0') {
        auto res = read_file_to_buffer(request.arg2, buffer);
        if (!res)
          return res;
      } else {
        read_terminal_to_buffer(buffer);
      }

      bool any_success = false;
      for (const auto &ss : replicas) {
        auto ss_sock_res = network::connect_to_server(ss.ip, ss.port_client);
        if (!ss_sock_res) {
          logger::warn("Could not connect to replica SS at " +
                       std::string(ss.ip));
          continue;
        }

        (void)network::send_all(*ss_sock_res, &request, sizeof(request));
        for (const auto &packet : buffer) {
          (void)network::send_all(*ss_sock_res, &packet, sizeof(packet));
        }
        any_success = true;
      }
      return any_success ? std::expected<void, Error>{}
                         : std::unexpected(Error::ConnectionFailed);
    }

    if (request.command == commands::Command::GET_FILE_INFO) {
      std::cout << "File: " << request.arg1 << "\n";
      std::cout << "Replica count: " << replicas.size() << "\n";
      for (const auto &ss : replicas) {
        std::cout << " - " << ss.ip << ":" << ss.port_client << " (SS ID: "
                  << ss.id << ")\n";
      }
      return {};
    }

    return {};
  }

  std::expected<void, Error>
  read_file_to_buffer(std::string_view local_path,
                      std::vector<commands::FilePacket> &buffer) {
    std::ifstream file{std::string(local_path), std::ios::binary};
    if (!file) {
      logger::error("Could not open local file: " + std::string(local_path));
      return std::unexpected(Error::FileNotFound);
    }

    commands::FilePacket packet;
    while (file.read(packet.chunk, sizeof(packet.chunk))) {
      packet.size = file.gcount();
      packet.is_last = file.peek() == EOF;
      buffer.push_back(packet);
      if (packet.is_last)
        return {};
    }

    if (file.gcount() > 0 || file.eof()) {
      packet.size = file.gcount();
      packet.is_last = true;
      buffer.push_back(packet);
    }
    return {};
  }

  void read_terminal_to_buffer(std::vector<commands::FilePacket> &buffer) {
    std::cout << "Enter content (type 'END' on a new line to finish):\n";
    std::string line;
    commands::FilePacket packet;
    while (std::getline(std::cin, line)) {
      if (line == "END")
        break;
      line += "\n";
      size_t pos = 0;
      while (pos < line.size()) {
        size_t to_copy = std::min(line.size() - pos, sizeof(packet.chunk));
        std::memcpy(packet.chunk, line.data() + pos, to_copy);
        packet.size = to_copy;
        pos += to_copy;
        packet.is_last = false;
        buffer.push_back(packet);
      }
    }
    std::memset(packet.chunk, 0, sizeof(packet.chunk));
    packet.size = 0;
    packet.is_last = true;
    buffer.push_back(packet);
  }

  std::expected<void, Error>
  receive_file_stream(const network::Socket &ss_sock) {
    commands::FilePacket packet;
    while (true) {
      auto recv_res = network::receive_all(ss_sock, &packet, sizeof(packet));
      if (!recv_res)
        break;
      if (packet.size > 0) {
        std::cout.write(packet.chunk, packet.size);
      }
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
    (void)network::send_all(*ns_socket, &request, sizeof(request));
    commands::AckPacket ack;
    auto recv_res = network::receive_all(*ns_socket, &ack, sizeof(ack));
    if (!recv_res || ack.status != commands::Status::Success) {
      if (recv_res) {
        logger::error("Naming Server returned error code: " +
                      std::to_string(ack.error_code));
      }
      return std::unexpected(Error::InvalidCommand);
    }
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
  int retries = 0;
  const int max_retries = 10;
  
  while (retries < max_retries) {
    auto sock_res = network::connect_to_server(std::string(ip), port);
    if (sock_res) {
      impl_->ns_socket = std::move(*sock_res);
      return {};
    }
    
    retries++;
    logger::warn("Client: Failed to connect to Naming Server (attempt " + 
                 std::to_string(retries) + "/" + std::to_string(max_retries) + "). Retrying in 2s...");
    std::this_thread::sleep_for(std::chrono::seconds(2));
  }

  return std::unexpected(Error::ConnectionFailed);
}

std::expected<void, Error> Client::run_interactive_loop() {
  std::string line;
  while (true) {
    std::cout << "DFS> " << std::flush;
    if (!std::getline(std::cin, line) || line == "exit")
      break;
    
    if (line.empty() || line.find_first_not_of(" \t\n\r") == std::string::npos)
      continue;

    auto res = impl_->process_command(line);
    if (!res)
      logger::error("Command failed.");
  }
  return {};
}

} // namespace client
