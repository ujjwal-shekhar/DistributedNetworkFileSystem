#include <chrono>
#include <expected>
#include <iostream>
#include <string>
#include <thread>

import storage_server;
import logger;

int main(int argc, char *argv[]) {
  std::string nm_ip = "127.0.0.1";
  int nm_port = 8080;
  std::string storage_root = "";

  if (argc >= 2)
    storage_root = argv[1];
  if (argc >= 3)
    nm_ip = argv[2];
  if (argc >= 4) {
    try {
      nm_port = std::stoi(argv[3]);
    } catch (...) {
      std::cerr << "Invalid nm_port, defaulting to 8080.\n";
    }
  }

  storage::Config config{.nm_ip = nm_ip,
                         .nm_port = nm_port,
                         .client_port = 0,  // Ephemeral
                         .nm_comm_port = 0, // Ephemeral
                         .storage_root = storage_root,
                         .accessible_paths = {}};

  storage::StorageServer server(config);

  auto start_res = server.start();
  if (!start_res) {
    logger::error("Failed to start Storage Server.");
    return 1;
  }

  logger::info("Storage Server operational.");
  while (true) {
    std::this_thread::sleep_for(std::chrono::hours(1));
  }
  return 0;
}
