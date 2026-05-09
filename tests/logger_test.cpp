#include <gtest/gtest.h>
import logger;

TEST(LoggerTest, BasicLog) {
  logger::info("Test info message");
  logger::warn("Test warning message");
  logger::error("Test error message");
  logger::debug("Test debug message");
  SUCCEED();
}
