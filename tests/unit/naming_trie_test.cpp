#include <gtest/gtest.h>
#include <vector>
#include <string>
#include <cstring>
import naming_server;
import commands;

TEST(NamingTrieTest, RegisterAndFindPath) {
  naming::NamingService service;
  std::vector<int> servers = {1, 2};
  service.register_path("a.txt", servers, true);

  auto res = service.find_storage_server("a.txt");
  ASSERT_TRUE(res.has_value());
  EXPECT_EQ(res.value(), servers);
}

TEST(NamingTrieTest, ListAll) {
  naming::NamingService service;
  service.register_path("dir/file1.txt", {1}, true);
  service.register_path("dir/file2.txt", {2}, true);
  service.register_path("other.txt", {1}, true);

  auto files = service.list_all();
  EXPECT_EQ(files.size(), 3);
  
  bool found = false;
  for (const auto& f : files) {
    if (f == "dir/file1.txt") found = true;
  }
  EXPECT_TRUE(found);
}

TEST(NamingTrieTest, RemovePathRecursive) {
  naming::NamingService service;
  service.register_path("X/a.txt", {1}, true);
  service.register_path("X/b.txt", {1}, true);
  
  service.remove_path("X/a.txt");
  auto res = service.find_storage_server("X/a.txt");
  EXPECT_FALSE(res.has_value());
  
  res = service.find_storage_server("X/b.txt");
  EXPECT_TRUE(res.has_value());
}

TEST(NamingTrieTest, UnderReplicationDetection) {
  naming::NamingService service;
  commands::ServerDetails ss1{.id = 1, .online = true};
  commands::ServerDetails ss2{.id = 2, .online = true};
  service.register_server(ss1);
  service.register_server(ss2);

  service.register_path("replicated.txt", {1, 2}, true);
  service.register_path("under.txt", {1}, true);

  // Target factor 2: replicated.txt (2 online) is OK, under.txt (1 online) is NOT
  auto tasks = service.get_under_replicated_paths(2);
  ASSERT_EQ(tasks.size(), 1);
  EXPECT_EQ(tasks[0].path, "under.txt");
  EXPECT_EQ(tasks[0].current_online_servers.size(), 1);

  // Mark server 2 offline
  service.mark_server_offline(2);
  
  // Now replicated.txt also has only 1 online server
  tasks = service.get_under_replicated_paths(2);
  EXPECT_EQ(tasks.size(), 2);
}

TEST(NamingTrieTest, AddServerToPath) {
  naming::NamingService service;
  service.register_path("file.txt", {1}, true);
  
  service.add_server_to_path("file.txt", 2);
  
  auto res = service.find_storage_server("file.txt");
  ASSERT_TRUE(res.has_value());
  EXPECT_EQ(res->size(), 2);
  EXPECT_EQ((*res)[1], 2);
}

TEST(NamingTrieTest, JobServerRegistration) {
  naming::NamingService service;
  commands::ServerDetails js1{.id = -1, .online = true};
  int id = service.register_job_server(js1);
  EXPECT_GE(id, 1001);
  
  auto details = service.get_job_server_details(id);
  ASSERT_TRUE(details.has_value());
  EXPECT_TRUE(details->online);
  
  service.mark_job_server_offline(id);
  details = service.get_job_server_details(id);
  EXPECT_FALSE(details->online);
  EXPECT_EQ(service.count_online_job_servers(), 0);
}

TEST(NamingTrieTest, ComplexListAll) {
  naming::NamingService service;
  service.register_path("a/b/c/1.txt", {1}, true);
  service.register_path("a/b/d/2.txt", {1}, true);
  service.register_path("x/y/3.txt", {1}, true);

  auto files = service.list_all();
  EXPECT_EQ(files.size(), 3);
}
