#include <gtest/gtest.h>
#include <thread>
#include <chrono>
import network;

TEST(NetworkTest, ServerClientLoopback) {
  int port = 9091;
  
  auto server_thread = std::thread([port]() {
    auto server_sock_res = network::create_server_socket(port);
    if (!server_sock_res) return;
    auto& server_sock = *server_sock_res;
    
    auto accept_res = server_sock.accept();
    if (!accept_res) return;
    auto& [client_sock, ip] = *accept_res;
    
    char buffer[128]{};
    (void)network::receive_all(client_sock, buffer, 5);
    (void)network::send_all(client_sock, "WORLD", 5);
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  auto client_sock_res = network::connect_to_server("localhost", port);
  ASSERT_TRUE(client_sock_res.has_value());
  auto& client_sock = *client_sock_res;

  (void)network::send_all(client_sock, "HELLO", 5);
  char buffer[128]{};
  (void)network::receive_all(client_sock, buffer, 5);
  EXPECT_STREQ(buffer, "WORLD");

  server_thread.join();
}

TEST(NetworkTest, ConcurrentConnections) {
  int port = 9092;
  std::atomic<int> connections_handled = 0;

  auto server_thread = std::jthread([&](std::stop_token st) {
    auto server_sock_res = network::create_server_socket(port);
    if (!server_sock_res) return;
    auto& server_sock = *server_sock_res;

    while (!st.stop_requested()) {
      auto accept_res = server_sock.accept();
      if (!accept_res) continue;
      connections_handled++;
    }
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  const int NUM_CLIENTS = 10;
  std::vector<std::jthread> clients;
  for(int i = 0; i < NUM_CLIENTS; ++i) {
    clients.emplace_back([]() {
      auto res = network::connect_to_server("localhost", 9092);
      EXPECT_TRUE(res.has_value());
    });
  }

  clients.clear(); // Wait for all clients to finish connecting
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  server_thread.request_stop();
  
  // Try to connect once more to unblock accept if it's hanging
  (void)network::connect_to_server("localhost", 9092);

  EXPECT_GE(connections_handled.load(), NUM_CLIENTS);
}
