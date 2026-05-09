#include <expected>
#include <iostream>

import client;
import logger;

int main(int argc, char *argv[]) {
  std::string nm_ip = "127.0.0.1";
  int nm_port = 8080;

  if (argc >= 2) nm_ip = argv[1];
  if (argc >= 3) {
    try {
      nm_port = std::stoi(argv[2]);
    } catch (...) {}
  }

  logger::info("Starting Modern Distributed Network File System Client...");

  client::Client client;

  logger::info("Connecting to Naming Server at " + nm_ip + ":" + std::to_string(nm_port) + "...");
  auto conn_res = client.connect(nm_ip, nm_port);
  if (!conn_res) {
    logger::error("Failed to connect to Naming Server at " + nm_ip + ":" + std::to_string(nm_port));
    return 1;
  }

  auto loop_res = client.run_interactive_loop();
  if (!loop_res) {
    logger::error("Interactive loop terminated with error.");
    return 1;
  }

  logger::info("Client session ended.");
  return 0;
}
