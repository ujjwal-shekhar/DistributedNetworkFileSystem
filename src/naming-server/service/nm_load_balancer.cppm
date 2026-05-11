module;
#include <algorithm>
#include <random>
#include <string>
#include <vector>

export module naming_server:load_balancer;
import commands;

namespace naming::lb {

export enum class Policy { RANDOM, ROUND_ROBIN, LEAST_CONNECTIONS };

export struct JobServerInfo {
  std::string ip;
  int port;
  int active_tasks;
};

export class LoadBalancer {
public:
  static commands::ServerDetails
  select_js(const std::vector<commands::ServerDetails> &servers,
            Policy policy = Policy::ROUND_ROBIN) {
    if (servers.empty())
      return {};

    static size_t rr_index = 0;
    switch (policy) {
    case Policy::RANDOM: {
      static std::mt19937 rng(std::random_device{}());
      std::uniform_int_distribution<size_t> dist(0, servers.size() - 1);
      return servers[dist(rng)];
    }
    case Policy::ROUND_ROBIN: {
      auto s = servers[rr_index % servers.size()];
      rr_index = (rr_index + 1) % servers.size();
      return s;
    }
    case Policy::LEAST_CONNECTIONS:
      // For prototype, fallback to RR as we need JS state tracking
      return servers[0];
    }
    return servers[0];
  }
};

} // namespace naming::lb
