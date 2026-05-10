#include <iostream>
#include <string>
#include <vector>

import job_server;
import logger;

int main(int argc, char *argv[]) {
  std::string nm_ip = "127.0.0.1";
  int nm_reg_port = 6051; // Default JS registration port
  int nm_clt_port = 8080; // Default NM client port

  if (argc >= 2)
    nm_ip = argv[1];
  if (argc >= 3) {
    try {
      nm_reg_port = std::stoi(argv[2]);
    } catch (...) {
    }
  }
  if (argc >= 4) {
    try {
      nm_clt_port = std::stoi(argv[3]);
    } catch (...) {
    }
  }

  logger::info("Starting Modern DNFS Job Server...");

  job_server::JobServer server;
  auto res = server.run(nm_ip, nm_reg_port, nm_clt_port);

  if (!res) {
    logger::error("Job Server failed to start.");
    return 1;
  }

  return 0;
}
