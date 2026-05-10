#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <string>
#include <vector>

import client;
import commands;

using namespace client;

// --- Argument Splitting Tests ---
TEST(JobUtilsTest, SplitArgsSimple) {
  auto args = utils::split_args("grep -i \"hello world\" file.txt");
  ASSERT_EQ(args.size(), 4);
  EXPECT_EQ(args[0], "grep");
  EXPECT_EQ(args[1], "-i");
  EXPECT_EQ(args[2], "hello world");
  EXPECT_EQ(args[3], "file.txt");
}

TEST(JobUtilsTest, SplitArgsSingleQuotes) {
  auto args = utils::split_args("awk '{print $1}' data.log");
  ASSERT_EQ(args.size(), 3);
  EXPECT_EQ(args[0], "awk");
  EXPECT_EQ(args[1], "{print $1}");
  EXPECT_EQ(args[2], "data.log");
}

TEST(JobUtilsTest, SplitArgsEmpty) {
  auto args = utils::split_args("   ");
  EXPECT_TRUE(args.empty());
}

TEST(JobUtilsTest, SplitArgsNoSpaces) {
  auto args = utils::split_args("wc");
  ASSERT_EQ(args.size(), 1);
  EXPECT_EQ(args[0], "wc");
}

// --- Protocol Tests ---
TEST(JobProtocolTest, PayloadSize) {
  // Ensure our JobPayload is POD and has expected limits
  EXPECT_EQ(sizeof(commands::JobPayload),
            commands::MAX_ARG_LEN + commands::MAX_PATH_LEN);
}

// --- Job Logic Tests (Internal Helper Logic) ---
TEST(JobServerTest, ResolvePathConsistency) {
  // JS depends on client::utils for path resolution in some areas
  std::string cwd = "/home/user";
  EXPECT_EQ(utils::resolve_path(cwd, "file.txt"), "/home/user/file.txt");
  EXPECT_EQ(utils::resolve_path(cwd, "../other/data.csv"),
            "/home/other/data.csv");
}

TEST(JobServerTest, RedirectionRejection) {
  // Redirections should be caught by client before submission
  EXPECT_TRUE(utils::contains_redirect("job wc > out.txt"));
  EXPECT_TRUE(utils::contains_redirect("job sort < in.txt"));
  EXPECT_FALSE(utils::contains_redirect("job grep -r \"foo\" ."));
}
