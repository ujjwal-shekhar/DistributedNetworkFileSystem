#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <thread>
import storage_server;

namespace fs = std::filesystem;

class FileSystemTest : public ::testing::Test {
protected:
  void Set_Up() {
    fs::create_directories("test_root");
    fs::current_path("test_root");
  }

  void Tear_Down() {
    fs::current_path("..");
    fs::remove_all("test_root");
  }
};

TEST(LockManagerTest, BasicLocking) {
  storage::LockManager locks;
  std::string path = "test_file.txt";

  // Test that we can acquire and release locks without crashing/deadlocking
  locks.acquire_write(path);
  locks.release_write(path);

  locks.acquire_read(path);
  locks.release_read(path);
}

TEST(LockManagerTest, ConcurrentAccess) {
  storage::LockManager locks;
  std::string path = "shared.txt";
  int counter = 0;

  auto worker = [&]() {
    for(int i = 0; i < 100; ++i) {
      locks.acquire_write(path);
      counter++;
      locks.release_write(path);
    }
  };

  std::thread t1(worker);
  std::thread t2(worker);
  t1.join();
  t2.join();

  EXPECT_EQ(counter, 200);
}

TEST(StorageFileSystemTest, CreateFileAndDir) {
  // Use a unique subdir for this test to avoid collisions if run in parallel
  std::string root = "storage_test_fs";
  fs::create_directories(root);
  auto old_path = fs::current_path();
  fs::current_path(root);

  auto res = storage::FileSystem::create_directory("sub");
  EXPECT_TRUE(res.has_value());
  EXPECT_TRUE(fs::is_directory("sub"));

  res = storage::FileSystem::create_file("sub/file.txt");
  EXPECT_TRUE(res.has_value());
  EXPECT_TRUE(fs::exists("sub/file.txt"));

  // Cleanup
  fs::current_path(old_path);
  fs::remove_all(root);
}
