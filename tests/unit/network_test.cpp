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

TEST(NetworkTest, HostnameResolution) {
  // localhost should always resolve
  auto res = network::connect_to_server("localhost", 9999); // Port doesn't matter for resolution failure check
  // Resolution should succeed, but connect should fail (since nothing is listening)
  if (!res) {
    EXPECT_EQ(res.error(), network::Error::ConnectFailed);
  }
}
