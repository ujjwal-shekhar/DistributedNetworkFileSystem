#include <gtest/gtest.h>
#include <thread>
#include <vector>
import logger;

TEST(LoggerTest, ConcurrentLogging) {
  std::vector<std::jthread> threads;
  for(int i = 0; i < 10; ++i) {
    threads.emplace_back([]() {
      for(int j = 0; j < 50; ++j) {
        logger::info("Concurrent log message");
      }
    });
  }
  SUCCEED();
}
