module;

#include <cstring>
#include <expected>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

module job_server;

import network;
import commands;
import logger;
import client;

namespace job_server {

struct JobServer::Impl {
  std::optional<network::Socket> nm_socket;
  int assigned_id = -1;
  int listen_port = 0;
  std::string nm_ip;
  int nm_clt_port = 0;

  std::expected<void, Error> register_with_nm(std::string_view ip, int reg_port,
                                              int clt_port) {
    nm_ip = std::string(ip);
    nm_clt_port = clt_port;

    auto sock_res = network::connect_to_server(nm_ip, reg_port);
    if (!sock_res)
      return std::unexpected(Error::ConnectionFailed);
    nm_socket = std::move(*sock_res);

    commands::ServerDetails details{};
    details.id = -1;
    details.port_nm = listen_port; // Port NM uses to send us tasks
    // IP will be detected by NM

    (void)network::send_all(*nm_socket, &details, sizeof(details));
    (void)network::receive_all(*nm_socket, &assigned_id, sizeof(assigned_id));

    if (assigned_id == -1)
      return std::unexpected(Error::RegistrationFailed);

    logger::info("Job Server: Registered with ID " +
                 std::to_string(assigned_id));
    return {};
  }

  void handle_task(network::Socket nm_task_sock) {
    commands::ClientRequest task_req;
    if (!network::receive_all(nm_task_sock, &task_req, sizeof(task_req)))
      return;

    commands::JobPayload payload;
    if (!network::receive_all(nm_task_sock, &payload, sizeof(payload)))
      return;

    logger::info(
        "Job Server: Executing task: " + std::string(payload.command_line) +
        " on " + std::string(payload.target_file));

    // 1. Get file location from NM
    commands::ClientRequest loc_req{};
    loc_req.command = commands::Command::GET_FILE_INFO;
    std::strncpy(loc_req.arg1, payload.target_file, sizeof(loc_req.arg1) - 1);

    // Use stored nm_ip and nm_clt_port instead of hardcoded values
    auto nm_info_sock_res = network::connect_to_server(nm_ip, nm_clt_port);
    if (!nm_info_sock_res) {
      send_error(nm_task_sock, "Could not connect to NM for file info");
      return;
    }
    (void)network::send_all(*nm_info_sock_res, &loc_req, sizeof(loc_req));

    commands::AckPacket ack;
    (void)network::receive_all(*nm_info_sock_res, &ack, sizeof(ack));
    if (ack.status != commands::Status::Success || ack.extra_count == 0) {
      send_error(nm_task_sock, "File not found or no replicas online");
      return;
    }

    commands::ServerDetails ss_details;
    (void)network::receive_all(*nm_info_sock_res, &ss_details,
                               sizeof(ss_details));

    // 2. Pull file from SS
    std::string local_tmp = "/tmp/js_task_" + std::to_string(assigned_id) +
                            "_" + std::to_string(getpid());
    if (!pull_file(ss_details, payload.target_file, local_tmp)) {
      send_error(nm_task_sock, "Failed to pull file from SS");
      return;
    }

    // 3. Execute job
    execute_job(nm_task_sock, payload.command_line, local_tmp);

    // Cleanup
    std::filesystem::remove(local_tmp);
  }

  bool pull_file(const commands::ServerDetails &ss, const char *remote_path,
                 const std::string &local_tmp) {
    auto ss_sock_res = network::connect_to_server(ss.ip, ss.port_client);
    if (!ss_sock_res)
      return false;

    commands::ClientRequest req{};
    req.command = commands::Command::READ_FILE;
    std::strncpy(req.arg1, remote_path, sizeof(req.arg1) - 1);
    (void)network::send_all(*ss_sock_res, &req, sizeof(req));

    std::ofstream file(local_tmp, std::ios::binary);
    if (!file)
      return false;

    commands::FilePacket packet;
    while (true) {
      auto r = network::receive_all(*ss_sock_res, &packet, sizeof(packet));
      if (!r)
        break;
      if (packet.size > 0)
        file.write(packet.chunk, packet.size);
      if (packet.is_last)
        break;
    }
    return true;
  }

  void execute_job(network::Socket &nm_task_sock, const char *cmd_line,
                   const std::string &data_file) {
    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) {
      send_error(nm_task_sock, "Internal pipe error");
      return;
    }

    pid_t pid = fork();
    if (pid == 0) { // Child
      close(pipe_fd[0]);
      dup2(pipe_fd[1], STDOUT_FILENO);
      dup2(pipe_fd[1], STDERR_FILENO);

      // Redirect stdin from data_file
      int data_fd = open(data_file.c_str(), O_RDONLY);
      if (data_fd != -1) {
        dup2(data_fd, STDIN_FILENO);
        close(data_fd);
      }

      // Split cmd_line into argv using shared utils
      auto args = client::utils::split_args(cmd_line);
      std::vector<char*> argv;
      for (auto& s : args) argv.push_back(const_cast<char*>(s.c_str()));
      argv.push_back(nullptr);

      execvp(argv[0], argv.data());
      exit(1); // Should not reach here

    } else if (pid > 0) { // Parent
      close(pipe_fd[1]);

      commands::AckPacket ack{.status = commands::Status::Success};
      (void)network::send_all(nm_task_sock, &ack, sizeof(ack));

      char buffer[commands::FILE_CHUNK_SIZE];
      ssize_t bytes_read;
      while ((bytes_read = read(pipe_fd[0], buffer, sizeof(buffer))) > 0) {
        commands::FilePacket packet{};
        std::memcpy(packet.chunk, buffer, bytes_read);
        packet.size = bytes_read;
        packet.is_last = false;
        (void)network::send_all(nm_task_sock, &packet, sizeof(packet));
      }

      commands::FilePacket last_packet{};
      last_packet.size = 0;
      last_packet.is_last = true;
      (void)network::send_all(nm_task_sock, &last_packet, sizeof(last_packet));

      close(pipe_fd[0]);
      waitpid(pid, nullptr, 0);
    }
  }

  void send_error(network::Socket &sock, std::string_view msg) {
    logger::error("Job Server: " + std::string(msg));
    commands::AckPacket ack{.status = commands::Status::Error};
    (void)network::send_all(sock, &ack, sizeof(ack));
  }
};

JobServer::JobServer() : impl_(std::make_unique<Impl>()) {}
JobServer::~JobServer() = default;

std::expected<void, Error> JobServer::run(std::string_view nm_ip,
                                          int nm_reg_port, int nm_clt_port) {
  // 1. Create task listener socket
  auto listen_sock_res =
      network::create_server_socket(0); // Bind to ephemeral port
  if (!listen_sock_res)
    return std::unexpected(Error::NetworkError);

  auto port_res = listen_sock_res->get_port();
  if (!port_res)
    return std::unexpected(Error::NetworkError);
  impl_->listen_port = *port_res;

  // 2. Register
  auto reg_res = impl_->register_with_nm(nm_ip, nm_reg_port, nm_clt_port);
  if (!reg_res)
    return std::unexpected(reg_res.error());

  // 3. Listen for tasks
  logger::info("Job Server: Listening for tasks on port " +
               std::to_string(impl_->listen_port));
  while (true) {
    auto accept_res = listen_sock_res->accept();
    if (!accept_res)
      continue;
    auto [sock, peer_ip] = std::move(*accept_res);
    std::jthread([this, s = std::move(sock)]() mutable {
      impl_->handle_task(std::move(s));
    }).detach();
  }
  return {};
}

} // namespace job_server
