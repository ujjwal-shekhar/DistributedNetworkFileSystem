module;

#include <chrono>
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
#include <vector>

module client;

import :utils;
import :prompt;
import :warp;
import :peek;
import :pastevents;

import network;
import naming_server;
import logger;
import commands;

namespace client {

struct Client::Impl {
  std::optional<network::Socket> ns_socket;
  Session session;
  pastevents::History history;
  std::string logical_cwd = "/";
  ShellStatus last_status = ShellStatus::Success;

  explicit Impl(size_t history_size) : history(history_size) {}

  std::expected<void, Error> process_line(std::string_view line) {
    auto commands_list = utils::split_commands(line);

    for (const auto &cmd_str : commands_list) {
      if (cmd_str.empty())
        continue;

      // Only route to DAG if explicitly a distributed job request
      bool is_explicit_job = cmd_str.starts_with("job ");
      bool has_dag_ops = utils::contains_pipe(cmd_str) || 
                         utils::contains_redirect(cmd_str) ||
                         utils::contains_async(cmd_str);

      if (is_explicit_job || has_dag_ops) {
        auto res = handle_dag_command(cmd_str);
        if (!res) {
          logger::error("DAG execution failed for: " + cmd_str);
          last_status = ShellStatus::Error;
        }
        continue;
      }

      history.add(cmd_str);
      last_status = ShellStatus::Success;
      auto res = process_single_command(cmd_str);
      if (!res) {
        logger::error("Command failed: " + cmd_str);
        last_status = ShellStatus::Error;
      }
    }
    return {};
  }


  std::expected<void, Error> handle_dag_command(std::string_view line) {
    history.add(line);
    std::cout << "\033[1;34mCompiling DAG...\033[0m\n";
    auto payload = utils::parse_dag(line, logical_cwd);

    // Basic validation: if no nodes, parsing failed (e.g. invalid syntax)
    if (payload.node_count == 0) {
        last_status = ShellStatus::Error;
        return std::unexpected(Error::InvalidCommand);
    }

    commands::ClientRequest request{};
    request.command = commands::Command::SUBMIT_DAG;

    if (!ns_socket) {
      last_status = ShellStatus::Error;
      return std::unexpected(Error::ConnectionFailed);
    }

    (void)network::send_all(*ns_socket, &request, sizeof(request));
    (void)network::send_all(*ns_socket, &payload, sizeof(payload));

    commands::AckPacket ack;
    auto recv_ack = network::receive_all(*ns_socket, &ack, sizeof(ack));
    if (!recv_ack || ack.status != commands::Status::Success) {
      std::cout << "\033[1;31mDAG Job Submission Failed.\033[0m\n";
      last_status = ShellStatus::Error;
      return std::unexpected(Error::InvalidCommand);
    }

    // Receive and print output stream from the DAG execution
    commands::FilePacket packet;
    while (true) {
      auto r = network::receive_all(*ns_socket, &packet, sizeof(packet));
      if (!r) {
        last_status = ShellStatus::Error;
        break;
      }
      if (packet.size > 0) {
        std::cout.write(packet.chunk, packet.size);
      }
      if (packet.is_last)
        break;
    }
    std::cout << std::endl;
    if (last_status != ShellStatus::Error)
        last_status = ShellStatus::Success;
    return {};
  }


  std::expected<void, Error> process_single_command(std::string_view line) {
    std::stringstream ss(std::string{line});
    std::string cmd_str;
    ss >> cmd_str;

    if (cmd_str == "warp") {
      std::string path;
      if (!(ss >> path))
        path = "~";
      logical_cwd = warp::execute(logical_cwd, path);
      return {};
    }

    if (cmd_str == "peek") {
      commands::ClientRequest request{};
      request.command = peek::get_translated_command();
      std::string arg;
      if (!(ss >> arg))
        arg = ".";

      std::string resolved = utils::resolve_path(logical_cwd, arg);
      std::strncpy(request.arg1, resolved.c_str(), sizeof(request.arg1) - 1);
      return handle_user_command(request);
    }

    if (cmd_str == "pastevents") {
      std::string sub;
      if (!(ss >> sub)) {
        for (const auto &event : history.get_all()) {
          std::cout << event << "\n";
        }
      } else if (sub == "purge") {
        history.purge();
        std::cout << "History purged.\n";
      } else if (sub == "execute") {
        int index;
        if (ss >> index) {
          std::string cmd = history.get_by_index(index);
          if (!cmd.empty()) {
            std::cout << "Executing: " << cmd << "\n";
            return process_single_command(cmd);
          } else {
            std::cout << "Invalid index.\n";
            last_status = ShellStatus::Error;
          }
        }
      }
      return {};
    }

    if (cmd_str == "job") {
      auto tokens = utils::split_args(line);
      if (tokens.size() < 3) {
        std::cout << "Usage: job <command> [args...] <target_file>\n";
        last_status = ShellStatus::Warning;
        return {};
      }

      std::string target_file = tokens.back();
      tokens.pop_back();            // Remove file
      tokens.erase(tokens.begin()); // Remove "job"

      std::string full_cmd;
      for (size_t i = 0; i < tokens.size(); ++i) {
        // Re-wrap tokens that might have had spaces in them
        if (tokens[i].find(' ') != std::string::npos) {
          full_cmd += "\"" + tokens[i] + "\"";
        } else {
          full_cmd += tokens[i];
        }
        if (i < tokens.size() - 1)
          full_cmd += " ";
      }

      commands::ClientRequest request{};
      request.command = commands::Command::SUBMIT_JOB;

      commands::JobPayload payload{};
      std::strncpy(payload.command_line, full_cmd.c_str(),
                   sizeof(payload.command_line) - 1);
      std::string resolved = utils::resolve_path(logical_cwd, target_file);
      std::strncpy(payload.target_file, resolved.c_str(),
                   sizeof(payload.target_file) - 1);

      if (!ns_socket)
        return std::unexpected(Error::ConnectionFailed);
      (void)network::send_all(*ns_socket, &request, sizeof(request));
      (void)network::send_all(*ns_socket, &payload, sizeof(payload));

      commands::AckPacket ack;
      auto recv_ack = network::receive_all(*ns_socket, &ack, sizeof(ack));
      if (!recv_ack || ack.status != commands::Status::Success) {
        std::cout << "\033[1;31mJob Submission Failed.\033[0m\n";
        last_status = ShellStatus::Error;
        return std::unexpected(Error::InvalidCommand);
      }

      // Receive and print output stream
      commands::FilePacket packet;
      while (true) {
        auto r = network::receive_all(*ns_socket, &packet, sizeof(packet));
        if (!r)
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

    auto cmd_opt = commands::string_to_command(cmd_str);
    if (!cmd_opt) {
      display_help();
      last_status = ShellStatus::Warning;
      return {};
    }

    commands::Command cmd = *cmd_opt;
    auto meta = commands::get_metadata(cmd);

    commands::ClientRequest request{};
    request.command = cmd;

    std::string arg1, arg2;
    if (ss >> arg1) {
      std::string resolved = utils::resolve_path(logical_cwd, arg1);
      std::strncpy(request.arg1, resolved.c_str(), sizeof(request.arg1) - 1);
    }
    if (ss >> arg2) {
      std::string resolved = utils::resolve_path(logical_cwd, arg2);
      std::strncpy(request.arg2, resolved.c_str(), sizeof(request.arg2) - 1);
    }

    switch (meta.tier) {
    case commands::PrivilegeTier::USER: {
      auto res = handle_user_command(request);
      if (res)
        std::cout << "\033[1;32mCommand Success.\033[0m\n";
      else {
        std::cout << "\033[1;31mCommand Failed.\033[0m\n";
        last_status = ShellStatus::Error;
      }
      return res;
    }
    case commands::PrivilegeTier::PRIVILEGED: {
      auto res = handle_privileged_command(request);
      if (res)
        std::cout << "\033[1;32mCommand Success.\033[0m\n";
      else {
        std::cout << "\033[1;31mCommand Failed.\033[0m\n";
        last_status = ShellStatus::Error;
      }
      return res;
    }
    case commands::PrivilegeTier::ADMIN: {
      auto res = handle_admin_command(request);
      if (res)
        std::cout << "\033[1;32mCommand Success.\033[0m\n";
      else {
        std::cout << "\033[1;31mCommand Failed.\033[0m\n";
        last_status = ShellStatus::Error;
      }
      return res;
    }
    }
    return {};
  }

  void display_help() {
    std::cout << "Available Commands:\n";
    std::cout << "  warp <path>       : Change local directory (client-side)\n";
    std::cout << "  peek [path]       : List files in directory (DNFS)\n";
    std::cout << "  pastevents        : Show command history\n";
    std::cout << "  pastevents purge  : Clear command history\n";
    std::cout << "  pastevents execute <idx> : Execute cmd from history\n";
    std::cout << "  job <cmd> [args] <file> : Execute distributed job\n";
    std::cout << "  help              : Show this help message\n";
    std::cout << "  LIST_ALL          : List all files\n";
    std::cout << "  READ_FILE <path>  : Read file content\n";
    std::cout << "  WRITE_FILE <p1> [p2] : Write to file\n";
    std::cout << "  CREATE_FILE <path> : Create a new file\n";
    std::cout << "  CREATE_DIR <path>  : Create a new directory\n";
    std::cout << "  DELETE_FILE <path> : Delete a file\n";
    std::cout << "  DELETE_DIR <path>  : Delete a directory\n";
    std::cout << "  GET_FILE_INFO <path> : Get file metadata\n";
    std::cout << "  FAIL_SERVER       : Simulate server failure (Admin)\n";
    std::cout << "  exit              : Exit the shell\n";
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
        std::cout << " - " << ss.ip << ":" << ss.port_client
                  << " (SS ID: " << ss.id << ")\n";
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

Client::Client(size_t history_size)
    : impl_(std::make_unique<Impl>(history_size)) {}
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
                 std::to_string(retries) + "/" + std::to_string(max_retries) +
                 "). Retrying in 2s...");
    std::this_thread::sleep_for(std::chrono::seconds(2));
  }

  return std::unexpected(Error::ConnectionFailed);
}

std::expected<void, Error> Client::run_interactive_loop() {
  while (true) {
    std::string prompt_str =
        prompt::get_prompt(impl_->logical_cwd, impl_->last_status);
    std::string line = input::read_line(impl_->history, prompt_str);

    if (line == "exit")
      break;

    if (line.empty() || line.find_first_not_of(" \t\n\r") == std::string::npos)
      continue;

    auto res = impl_->process_line(line);
    if (!res)
      logger::error("Command failed.");
  }
  return {};
}

} // namespace client
