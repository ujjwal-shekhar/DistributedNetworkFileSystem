#include <expected>
#include <iostream>
#include <string>
#include <vector>

import client;
import logger;

int main(int argc, char *argv[]) {
  std::string nm_ip = "127.0.0.1";
  int nm_port = 8080;
  size_t history_size = 5;

  std::vector<std::string> args(argv + 1, argv + argc);
  for (size_t i = 0; i < args.size(); ++i) {
    if (args[i] == "--history-size" && i + 1 < args.size()) {
      try {
        history_size = std::stoul(args[++i]);
      } catch (...) {
      }
    } else if (i == 0 && args[i][0] != '-') {
      nm_ip = args[i];
    } else if (i == 1 && args[i][0] != '-') {
      try {
        nm_port = std::stoi(args[i]);
      } catch (...) {
      }
    }
  }

  logger::set_log_file("client.log");
  std::cout << "Starting DNFS Client..." << std::endl;
  logger::info("Starting Modern Distributed Network File System Client...");

  client::Client client(history_size);

  std::cout << "Connecting to Naming Server at " << nm_ip << ":" << nm_port
            << "..." << std::endl;
  logger::info("Connecting to Naming Server at " + nm_ip + ":" +
               std::to_string(nm_port) + "...");
  auto conn_res = client.connect(nm_ip, nm_port);
  if (!conn_res) {
    logger::error("Failed to connect to Naming Server at " + nm_ip + ":" +
                  std::to_string(nm_port));
    std::cerr
        << "Error: Could not connect to Naming Server after multiple attempts."
        << std::endl;
    return 1;
  }

  std::cout << "Connected! Entering interactive shell." << std::endl;

  auto loop_res = client.run_interactive_loop();
  if (!loop_res) {
    logger::error("Interactive loop terminated with error.");
    return 1;
  }

  logger::info("Client session ended.");
  return 0;
}
