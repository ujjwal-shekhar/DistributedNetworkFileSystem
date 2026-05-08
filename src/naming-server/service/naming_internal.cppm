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

// FEEDBACK; importing in a GMF, please check if this is fine?
import commands;

export module naming_server:internal;

import core;

namespace naming {

struct TrieNode {
  std::unordered_map<char, std::unique_ptr<TrieNode>> children;
  int server_id = -1;
  bool is_file = false;
  bool is_end_of_word = false;
};

/**
 * @brief Two-Tier Cache: Limited size MRU/LRU with fast lookup.
 */
class LRUCache {
public:
  explicit LRUCache(size_t capacity) : capacity_(capacity) {}

  [[nodiscard]] std::optional<int> get(std::string_view path) {
    std::lock_guard lock(mutex_);
    auto it = map_.find(std::string(path));
    if (it == map_.end())
      return std::nullopt;

    list_.splice(list_.begin(), list_, it->second);
    return it->second->second;
  }

  void put(std::string_view path, int server_id) {
    std::lock_guard lock(mutex_);
    std::string key(path);
    auto it = map_.find(key);
    if (it != map_.end()) {
      list_.splice(list_.begin(), list_, it->second);
      it->second->second = server_id;
      return;
    }

    if (list_.size() >= capacity_) {
      auto last = list_.back();
      map_.erase(last.first);
      list_.pop_back();
    }

    list_.emplace_front(key, server_id);
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
  std::list<std::pair<std::string, int>> list_;
  std::unordered_map<std::string,
                     std::list<std::pair<std::string, int>>::iterator>
      map_;
  std::mutex mutex_;
};

} // namespace naming
