#include <expected>
#include <iostream>

import client;
import logger;

int main() {
  logger::info("Starting Modern Distributed Network File System Client...");

  client::Client client;

  auto conn_res = client.connect("127.0.0.1", 8080);
  if (!conn_res) {
    logger::error("Failed to connect to Naming Server.");
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
