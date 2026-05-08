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
};

NamingService::NamingService() : impl_(std::make_unique<Impl>()) {}
NamingService::~NamingService() = default;

ClientManager &NamingService::clients() noexcept {
  return impl_->client_manager;
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

std::expected<int, Error>
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

    if (curr->is_end_of_word && curr->server_id == -1) {
      return std::unexpected(Error::PathNotFound);
    }
  }

  if (curr->is_end_of_word) {
    impl_->cache.put(path, curr->server_id);
    return curr->server_id;
  }

  return std::unexpected(Error::PathNotFound);
}

void NamingService::register_path(std::string_view path, int server_id,
                                  bool is_file) {
  std::unique_lock lock(impl_->trie_mutex);
  register_path_internal(path, server_id, is_file);
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
  curr->server_id = server_id;
}

void NamingService::remove_path(std::string_view path) {
  std::unique_lock lock(impl_->trie_mutex);
  TrieNode *curr = &impl_->root;
  for (char c : path) {
    auto it = curr->children.find(c);
    if (it == curr->children.end())
      return;
    curr = it->second.get();
  }
  curr->server_id = -1;
  curr->is_end_of_word = false;
  impl_->cache.remove(path);
}

} // namespace naming
