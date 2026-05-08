#include <chrono>
#include <expected>
#include <iostream>
#include <string>
#include <thread>

import naming_server;
import logger;
import commands;

// Modernize the argparsing perhaps
int main(int argc, char *argv[]) {
  int min_ss = 3;
  if (argc > 1) {
    try {
      min_ss = std::stoi(argv[1]);
    } catch (...) {
      std::cerr << "Invalid min_ss value, defaulting to 3.\n";
    }
  }

  logger::info(
      "Starting Modern Distributed Network File System - Naming Server...");

  commands::Config config{
      .nm_clt_port = 8080, .nm_ss_reg_port = 5049, .nm_ss_comm_port = 4050};

  naming::NamingServer server(config);

  auto start_res = server.start(min_ss);
  if (!start_res) {
    logger::error("Failed to start Naming Server.");
    return 1;
  }

  logger::info("Naming Server is operational. Press Ctrl+C to stop.");

  while (true) {
    std::this_thread::sleep_for(std::chrono::hours(1));
  }

  return 0;
}
