module;

#include <cstddef>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

// FEEDBACK; importing in a GMF, please check if this is fine?
export module naming_server:internal;

import commands;

namespace naming {

struct TrieNode {
  std::unordered_map<char, std::unique_ptr<TrieNode>> children;
  std::vector<int> server_ids;
  bool is_file = false;
  bool is_end_of_word = false;
};

/**
 * @brief Two-Tier Cache: Limited size MRU/LRU with fast lookup.
 */
class LRUCache {
public:
  explicit LRUCache(size_t capacity) : capacity_(capacity) {}

  [[nodiscard]] std::optional<std::vector<int>> get(std::string_view path) {
    std::lock_guard lock(mutex_);
    auto it = map_.find(std::string(path));
    if (it == map_.end())
      return std::nullopt;

    list_.splice(list_.begin(), list_, it->second);
    return it->second->second;
  }

  void put(std::string_view path, const std::vector<int> &server_ids) {
    std::lock_guard lock(mutex_);
    std::string key(path);
    auto it = map_.find(key);
    if (it != map_.end()) {
      list_.splice(list_.begin(), list_, it->second);
      it->second->second = server_ids;
      return;
    }

    if (list_.size() >= capacity_) {
      auto last = list_.back();
      map_.erase(last.first);
      list_.pop_back();
    }

    list_.emplace_front(key, server_ids);
    map_[key] = list_.begin();
  }

  void remove(std::string_view path) {
    std::lock_guard lock(mutex_);
    auto it = map_.find(std::string(path));
    if (it != map_.end()) {
      list_.erase(it->second);
      map_.erase(it);
    }
  }

private:
  size_t capacity_;
  std::list<std::pair<std::string, std::vector<int>>> list_;
  std::unordered_map<
      std::string,
      std::list<std::pair<std::string, std::vector<int>>>::iterator>
      map_;
  std::mutex mutex_;
};

} // namespace naming
