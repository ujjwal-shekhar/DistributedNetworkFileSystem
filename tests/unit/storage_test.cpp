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

TEST(LockManagerTest, ReadWriteContention) {
  storage::LockManager locks;
  std::string path = "contention.txt";
  std::atomic<int> active_readers = 0;
  std::atomic<bool> writer_active = false;
  std::atomic<int> violations = 0;

  auto reader = [&]() {
    for(int i = 0; i < 50; ++i) {
      locks.acquire_read(path);
      active_readers++;
      if(writer_active) violations++;
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      active_readers--;
      locks.release_read(path);
    }
  };

  auto writer = [&]() {
    for(int i = 0; i < 10; ++i) {
      locks.acquire_write(path);
      writer_active = true;
      if(active_readers > 0) violations++;
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
      writer_active = false;
      locks.release_write(path);
    }
  };

  std::vector<std::jthread> threads;
  for(int i = 0; i < 5; ++i) threads.emplace_back(reader);
  threads.emplace_back(writer);
  
  threads.clear();
  EXPECT_EQ(violations.load(), 0);
}

TEST(StorageFileSystemTest, CreateDeleteCycle) {
  std::string root = "storage_cycle_test";
  fs::create_directories(root);
  auto old_path = fs::current_path();
  fs::current_path(root);

  // File creation/deletion
  EXPECT_TRUE(storage::FileSystem::create_file("test.txt").has_value());
  EXPECT_TRUE(fs::exists("test.txt"));
  EXPECT_TRUE(storage::FileSystem::delete_file("test.txt").has_value());
  EXPECT_FALSE(fs::exists("test.txt"));

  // Directory creation/deletion
  EXPECT_TRUE(storage::FileSystem::create_directory("nested/dir").has_value());
  EXPECT_TRUE(fs::is_directory("nested/dir"));
  EXPECT_TRUE(storage::FileSystem::delete_directory("nested").has_value());
  EXPECT_FALSE(fs::exists("nested"));

  fs::current_path(old_path);
  fs::remove_all(root);
}

TEST(StorageFileSystemTest, ListContents) {
  std::string root = "storage_list_test";
  fs::create_directories(root);
  auto old_path = fs::current_path();
  fs::current_path(root);

  (void)storage::FileSystem::create_directory("dir1");
  (void)storage::FileSystem::create_file("file1.txt");
  (void)storage::FileSystem::create_file("dir1/file2.txt");

  auto contents = storage::FileSystem::list_contents(".");
  EXPECT_EQ(contents.size(), 3);
  
  bool found_file2 = false;
  for (const auto& c : contents) {
    if (c.find("file2.txt") != std::string::npos) found_file2 = true;
  }
  EXPECT_TRUE(found_file2);

  fs::current_path(old_path);
  fs::remove_all(root);
}
