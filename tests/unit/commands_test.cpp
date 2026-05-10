#include <gtest/gtest.h>
import commands;

TEST(CommandsTest, MetadataLookup) {
  auto meta = commands::get_metadata(commands::Command::CREATE_FILE);
  EXPECT_EQ(meta.name, "CREATE_FILE");
  EXPECT_EQ(meta.tier, commands::PrivilegeTier::PRIVILEGED);
}

TEST(CommandsTest, StringToCommand) {
  auto cmd = commands::string_to_command("READ_FILE");
  ASSERT_TRUE(cmd.has_value());
  EXPECT_EQ(*cmd, commands::Command::READ_FILE);
  
  cmd = commands::string_to_command("LIST_FILES");
  ASSERT_TRUE(cmd.has_value());
  EXPECT_EQ(*cmd, commands::Command::LIST_ALL);

  cmd = commands::string_to_command("UNKNOWN_COMMAND");
  EXPECT_FALSE(cmd.has_value());
}
