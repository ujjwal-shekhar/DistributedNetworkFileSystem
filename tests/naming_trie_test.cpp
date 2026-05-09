#include <gtest/gtest.h>
#include <vector>
#include <string>
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
