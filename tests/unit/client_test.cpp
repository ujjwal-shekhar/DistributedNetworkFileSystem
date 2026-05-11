#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <string>
#include <vector>

import client;
import commands;

using namespace client;

// --- Utils Tests ---
TEST(ClientUtilsTest, SplitCommands) {
  auto cmds = utils::split_commands("warp test; peek file && help & LIST_ALL");
  ASSERT_EQ(cmds.size(), 4);
  EXPECT_EQ(cmds[0], "warp test");
  EXPECT_EQ(cmds[1], "peek file");
  EXPECT_EQ(cmds[2], "help");
  EXPECT_EQ(cmds[3], "LIST_ALL");
}

TEST(ClientUtilsTest, SplitCommandsEmpty) {
  auto cmds = utils::split_commands("  ;  &&  &  ");
  EXPECT_TRUE(cmds.empty());
}

TEST(ClientUtilsTest, ContainsPipeAndRedirect) {
  EXPECT_TRUE(utils::contains_pipe("ls | grep x"));
  EXPECT_FALSE(utils::contains_pipe("ls -la"));

  EXPECT_TRUE(utils::contains_redirect("cat x > y"));
  EXPECT_TRUE(utils::contains_redirect("sort < file"));
  EXPECT_FALSE(utils::contains_redirect("ls -la"));
}

TEST(ClientUtilsTest, ParseDAG) {
  auto payload = utils::parse_dag("ls | grep txt; wc -l", "/");
  
  // ls | grep txt -> 2 nodes, 1 pipe edge
  // ; wc -l -> 1 node, 1 sequence edge from grep txt
  ASSERT_EQ(payload.node_count, 3);
  ASSERT_EQ(payload.edge_count, 2);

  EXPECT_EQ(payload.nodes[0].type, commands::NodeType::COMMAND);
  EXPECT_STREQ(payload.nodes[0].command_line, "ls");

  EXPECT_EQ(payload.nodes[1].type, commands::NodeType::COMMAND);
  EXPECT_STREQ(payload.nodes[1].command_line, "grep txt");

  EXPECT_EQ(payload.nodes[2].type, commands::NodeType::COMMAND);
  EXPECT_STREQ(payload.nodes[2].command_line, "wc -l");

  EXPECT_EQ(payload.edges[0].type, commands::EdgeType::PIPE_DATA);
  EXPECT_EQ(payload.edges[0].from_id, 0);
  EXPECT_EQ(payload.edges[0].to_id, 1);

  EXPECT_EQ(payload.edges[1].type, commands::EdgeType::CONTROL_SEQ);
  EXPECT_EQ(payload.edges[1].from_id, 1);
  EXPECT_EQ(payload.edges[1].to_id, 2);
}

TEST(ClientUtilsTest, ParseDAGWithParentheses) {
  auto payload = utils::parse_dag("(ls | grep txt) ; wc -l", "/");
  
  ASSERT_EQ(payload.node_count, 3);
  ASSERT_EQ(payload.edge_count, 2);

  EXPECT_STREQ(payload.nodes[0].command_line, "ls");
  EXPECT_STREQ(payload.nodes[1].command_line, "grep txt");
  EXPECT_STREQ(payload.nodes[2].command_line, "wc -l");
}

TEST(ClientUtilsTest, ResolvePath) {
  EXPECT_EQ(utils::resolve_path("/", "test"), "/test");
  EXPECT_EQ(utils::resolve_path("/home", "user"), "/home/user");
  EXPECT_EQ(utils::resolve_path("/home/user", ".."), "/home");
  EXPECT_EQ(utils::resolve_path("/home/user", "../../.."), "/");
  EXPECT_EQ(utils::resolve_path("/", "/abs/path"), "/abs/path");
  EXPECT_EQ(utils::resolve_path("/a/b/c", "./d"), "/a/b/c/d");
  EXPECT_EQ(utils::resolve_path("/a/b/c", "../d"), "/a/b/d");
}

// --- Warp Tests ---
TEST(ClientWarpTest, ExecuteLogical) {
  std::string cwd = "/";
  cwd = warp::execute(cwd, "home");
  EXPECT_EQ(cwd, "/home");

  cwd = warp::execute(cwd, "user/docs");
  EXPECT_EQ(cwd, "/home/user/docs");

  cwd = warp::execute(cwd, "..");
  EXPECT_EQ(cwd, "/home/user");

  cwd = warp::execute(cwd, "~");
  EXPECT_EQ(cwd, "/");

  cwd = warp::execute(cwd, "/absolute");
  EXPECT_EQ(cwd, "/absolute");
}

// --- History Tests ---
class ClientHistoryTest : public ::testing::Test {
protected:
  void SetUp() override { std::filesystem::remove(".pastevents.log"); }
  void TearDown() override { std::filesystem::remove(".pastevents.log"); }
};

TEST_F(ClientHistoryTest, AddAndRetrieve) {
  pastevents::History history(3);
  history.add("cmd1");
  history.add("cmd2");
  history.add("cmd3");

  auto events = history.get_all();
  ASSERT_EQ(events.size(), 3);
  EXPECT_EQ(events[0], "cmd1");
  EXPECT_EQ(events[1], "cmd2");
  EXPECT_EQ(events[2], "cmd3");

  EXPECT_EQ(history.get_by_index(1), "cmd3");
  EXPECT_EQ(history.get_by_index(2), "cmd2");
  EXPECT_EQ(history.get_by_index(3), "cmd1");
}

TEST_F(ClientHistoryTest, DuplicatePrevention) {
  pastevents::History history(5);
  history.add("cmd1");
  history.add("cmd1"); // Should be ignored
  history.add("cmd2");
  history.add("cmd2"); // Should be ignored

  ASSERT_EQ(history.get_all().size(), 2);
  EXPECT_EQ(history.get_all()[0], "cmd1");
  EXPECT_EQ(history.get_all()[1], "cmd2");
}

TEST_F(ClientHistoryTest, MaxSizeLimiting) {
  pastevents::History history(2);
  history.add("cmd1");
  history.add("cmd2");
  history.add("cmd3"); // Should push out cmd1

  auto events = history.get_all();
  ASSERT_EQ(events.size(), 2);
  EXPECT_EQ(events[0], "cmd2");
  EXPECT_EQ(events[1], "cmd3");
}

TEST_F(ClientHistoryTest, Purge) {
  pastevents::History history(5);
  history.add("cmd1");
  history.purge();
  EXPECT_TRUE(history.get_all().empty());

  // Verify file is also purged
  std::ifstream file(".pastevents.log");
  std::string line;
  EXPECT_FALSE(std::getline(file, line));
}

TEST_F(ClientHistoryTest, Persistence) {
  {
    pastevents::History history(5);
    history.add("persistent_cmd");
  } // Desctructor/Closing should happen

  pastevents::History new_history(5);
  ASSERT_EQ(new_history.get_all().size(), 1);
  EXPECT_EQ(new_history.get_all()[0], "persistent_cmd");
}

// --- Prompt Tests ---
TEST(ClientPromptTest, Format) {
  // We can't easily check hostname/user since they vary,
  // but we can check the status colors and structure.
  std::string p = prompt::get_prompt("/test/path", ShellStatus::Success);
  EXPECT_NE(p.find("/test/path"), std::string::npos);
  EXPECT_NE(p.find("\033[1;32m^_^/*\033[0m"),
            std::string::npos); // Green Success

  p = prompt::get_prompt("/", ShellStatus::Error);
  EXPECT_NE(p.find("\033[1;31mv_v/*\033[0m"), std::string::npos); // Red Error

  p = prompt::get_prompt("/", ShellStatus::Warning);
  EXPECT_NE(p.find("\033[1;33m-_-/*\033[0m"),
            std::string::npos); // Yellow Warning
}
