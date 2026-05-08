module;

#include <mutex>
#include <string>
#include <unordered_map>

export module naming_server:client_manager;

namespace naming {

export struct ClientAccount {
  // FEEDBACK: Client absolutely doesn't need
  // to store its tokens, or worse the fact that it is
  // admin, on the client side. This should be purely naming server side info,
  // and the client should just send credentials or something
  // with each request.
  int tokens = 50;
  bool is_admin = false;
};

export class ClientManager {
public:
  [[nodiscard]] bool consume_tokens(const std::string &client_ip, int cost) {
    std::lock_guard lock(mutex_);
    auto &account = accounts_[client_ip];

    if (account.is_admin)
      return true;

    if (account.tokens >= cost) {
      account.tokens -= cost;
      return true;
    }
    return false;
  }

  void grant_admin(const std::string &client_ip) {
    std::lock_guard lock(mutex_);
    accounts_[client_ip].is_admin = true;
  }

  [[nodiscard]] int get_tokens(const std::string &client_ip) {
    std::lock_guard lock(mutex_);
    return accounts_[client_ip].tokens;
  }

private:
  std::unordered_map<std::string, ClientAccount> accounts_;
  std::mutex mutex_;
};

} // namespace naming
