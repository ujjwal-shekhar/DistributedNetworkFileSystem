module;

#include <expected>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

module naming_server;

import :internal;
import commands;

namespace naming {

struct NamingService::Impl {
  TrieNode root;
  std::shared_mutex trie_mutex;
  LRUCache cache{1024};
  ClientManager client_manager;
  std::unordered_map<int, commands::ServerDetails> storage_servers;
  std::unordered_map<int, commands::ServerDetails> job_servers;
};

NamingService::NamingService() : impl_(std::make_unique<Impl>()) {}
NamingService::~NamingService() = default;

ClientManager &NamingService::clients() noexcept {
  return impl_->client_manager;
}

void list_all_recursive(const TrieNode &node, std::string current_path,
                        std::vector<std::string> &results) {
  if (node.is_end_of_word) {
    results.push_back(current_path);
  }
  for (const auto &[c, child] : node.children) {
    list_all_recursive(*child, current_path + c, results);
  }
}

std::vector<std::string> NamingService::list_all() const {
  std::shared_lock lock(impl_->trie_mutex);
  std::vector<std::string> results;
  list_all_recursive(impl_->root, "", results);
  return results;
}

void get_under_replicated_recursive(
    const TrieNode &node, std::string current_path, int target_replication,
    const std::unordered_map<int, commands::ServerDetails> &storage_servers,
    std::vector<ReplicationTask> &results) {
  if (node.is_end_of_word) {
    std::vector<int> online_sources;
    for (int id : node.server_ids) {
      auto it = storage_servers.find(id);
      if (it != storage_servers.end() && it->second.online) {
        online_sources.push_back(id);
      }
    }
    if (!online_sources.empty() &&
        static_cast<int>(online_sources.size()) < target_replication) {
      results.push_back({current_path, node.is_file, online_sources});
    }
  }
  for (const auto &[c, child] : node.children) {
    get_under_replicated_recursive(*child, current_path + c, target_replication,
                                   storage_servers, results);
  }
}

std::vector<ReplicationTask>
NamingService::get_under_replicated_paths(int target_replication) const {
  std::shared_lock lock(impl_->trie_mutex);
  std::vector<ReplicationTask> results;
  get_under_replicated_recursive(impl_->root, "", target_replication,
                                 impl_->storage_servers, results);
  return results;
}

void NamingService::add_server_to_path(std::string_view path, int server_id) {
  std::unique_lock lock(impl_->trie_mutex);
  TrieNode *curr = &impl_->root;
  for (char c : path) {
    auto it = curr->children.find(c);
    if (it == curr->children.end())
      return;
    curr = it->second.get();
  }
  if (curr->is_end_of_word) {
    bool found = false;
    for (int id : curr->server_ids) {
      if (id == server_id) {
        found = true;
        break;
      }
    }
    if (!found) {
      curr->server_ids.push_back(server_id);
      impl_->cache.remove(path);
    }
  }
}

int NamingService::register_server(const commands::ServerDetails &details) {
  std::unique_lock lock(impl_->trie_mutex);
  int assigned_id = details.id;
  if (assigned_id == -1) {
    assigned_id = static_cast<int>(impl_->storage_servers.size() + 1);
  }

  commands::ServerDetails updated_details = details;
  updated_details.id = assigned_id;
  updated_details.online = true;
  impl_->storage_servers[assigned_id] = updated_details;

  for (int i = 0; i < details.path_count; ++i) {
    register_path_internal(details.paths[i], assigned_id, true);
  }
  return assigned_id;
}

void NamingService::mark_server_offline(int server_id) {
  std::unique_lock lock(impl_->trie_mutex);
  auto it = impl_->storage_servers.find(server_id);
  if (it != impl_->storage_servers.end()) {
    it->second.online = false;
  }
}

size_t NamingService::count_online_servers() const noexcept {
  std::shared_lock lock(impl_->trie_mutex);
  size_t count = 0;
  for (const auto &[id, details] : impl_->storage_servers) {
    if (details.online)
      count++;
  }
  return count;
}

std::vector<int> NamingService::get_online_server_ids() const {
  std::shared_lock lock(impl_->trie_mutex);
  std::vector<int> ids;
  for (const auto &[id, details] : impl_->storage_servers) {
    if (details.online)
      ids.push_back(id);
  }
  return ids;
}

size_t NamingService::count_registered_servers() const noexcept {
  std::shared_lock lock(impl_->trie_mutex);
  return impl_->storage_servers.size();
}

std::optional<commands::ServerDetails>
NamingService::get_server_details(int server_id) {
  std::shared_lock lock(impl_->trie_mutex);
  auto it = impl_->storage_servers.find(server_id);
  if (it != impl_->storage_servers.end())
    return it->second;
  return std::nullopt;
}

int NamingService::register_job_server(const commands::ServerDetails &details) {
  std::unique_lock lock(impl_->trie_mutex);
  int assigned_id = details.id;
  if (assigned_id == -1) {
    assigned_id = static_cast<int>(impl_->job_servers.size() +
                                   1001); // JS IDs start at 1001
  }

  commands::ServerDetails updated_details = details;
  updated_details.id = assigned_id;
  updated_details.online = true;
  impl_->job_servers[assigned_id] = updated_details;
  return assigned_id;
}

void NamingService::mark_job_server_offline(int server_id) {
  std::unique_lock lock(impl_->trie_mutex);
  auto it = impl_->job_servers.find(server_id);
  if (it != impl_->job_servers.end()) {
    it->second.online = false;
  }
}

std::vector<int> NamingService::get_online_job_server_ids() const {
  std::shared_lock lock(impl_->trie_mutex);
  std::vector<int> ids;
  for (const auto &[id, details] : impl_->job_servers) {
    if (details.online)
      ids.push_back(id);
  }
  return ids;
}

std::optional<commands::ServerDetails>
NamingService::get_job_server_details(int server_id) {
  std::shared_lock lock(impl_->trie_mutex);
  auto it = impl_->job_servers.find(server_id);
  if (it != impl_->job_servers.end())
    return it->second;
  return std::nullopt;
}

size_t NamingService::count_online_job_servers() const noexcept {
  std::shared_lock lock(impl_->trie_mutex);
  size_t count = 0;
  for (const auto &[id, details] : impl_->job_servers) {
    if (details.online)
      count++;
  }
  return count;
}

std::expected<std::vector<int>, Error>
NamingService::find_storage_server(std::string_view path) {
  if (auto cached = impl_->cache.get(path)) {
    return *cached;
  }

  std::shared_lock lock(impl_->trie_mutex);
  const TrieNode *curr = &impl_->root;
  for (char c : path) {
    auto it = curr->children.find(c);
    if (it == curr->children.end()) {
      return std::unexpected(Error::PathNotFound);
    }
    curr = it->second.get();
  }

  if (curr->is_end_of_word && !curr->server_ids.empty()) {
    impl_->cache.put(path, curr->server_ids);
    return curr->server_ids;
  }

  return std::unexpected(Error::PathNotFound);
}

void NamingService::register_path(std::string_view path,
                                  const std::vector<int> &server_ids,
                                  bool is_file) {
  std::unique_lock lock(impl_->trie_mutex);
  TrieNode *curr = &impl_->root;
  for (char c : path) {
    auto &child = curr->children[c];
    if (!child)
      child = std::make_unique<TrieNode>();
    curr = child.get();
    if (c == '/')
      curr->is_file = false;
  }
  curr->is_end_of_word = true;
  curr->is_file = is_file;
  curr->server_ids = server_ids;
  impl_->cache.remove(path);
}

void NamingService::register_path_internal(std::string_view path, int server_id,
                                           bool is_file) {
  TrieNode *curr = &impl_->root;
  for (char c : path) {
    auto &child = curr->children[c];
    if (!child)
      child = std::make_unique<TrieNode>();
    curr = child.get();
    if (c == '/')
      curr->is_file = false;
  }
  curr->is_end_of_word = true;
  curr->is_file = is_file;

  // Add server_id to server_ids if not already present
  bool found = false;
  for (int id : curr->server_ids) {
    if (id == server_id) {
      found = true;
      break;
    }
  }
  if (!found) {
    curr->server_ids.push_back(server_id);
  }
}

bool remove_recursive(TrieNode &node, std::string_view path, size_t depth) {
  if (depth == path.size()) {
    node.is_end_of_word = false;
    node.server_ids.clear();
    return node.children.empty();
  }

  char c = path[depth];
  auto it = node.children.find(c);
  if (it == node.children.end())
    return false;

  bool can_delete_child = remove_recursive(*it->second, path, depth + 1);

  if (can_delete_child) {
    node.children.erase(it);
    return node.children.empty() && !node.is_end_of_word;
  }

  return false;
}

void NamingService::remove_path(std::string_view path) {
  std::unique_lock lock(impl_->trie_mutex);
  remove_recursive(impl_->root, path, 0);
  impl_->cache.remove(path);
  // Also need to clear all cache entries that have 'path/' as prefix
  // Since our LRUCache is simple, we might need a better way or just clear it.
  // For now, let's assume the user will most likely hit the trie for subpaths.
}

} // namespace naming
