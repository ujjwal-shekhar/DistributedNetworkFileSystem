module;

#include <algorithm>
#include <atomic>
#include <cstring>
#include <expected>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <random>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

module naming_server;

import :internal;
import commands;
import logger;

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

void NamingService::process_dag(const commands::DAGPayload &payload,
                                const network::Socket &client_socket) {
  logger::info("Naming Server: Processing DAG with " +
               std::to_string(payload.node_count) + " nodes.");

  if (count_online_job_servers() == 0) {
    logger::error("Naming Server: Cannot process DAG, no Job Servers online.");
    commands::AckPacket ack{.status = commands::Status::Error,
                            .error_code = 503};
    (void)network::send_all(client_socket, &ack, sizeof(ack));
    return;
  }

  commands::AckPacket ack{.status = commands::Status::Success};
  (void)network::send_all(client_socket, &ack, sizeof(ack));

  std::vector<std::vector<int>> adj(payload.node_count);
  std::vector<int> in_degree(payload.node_count, 0);
  std::vector<std::string> node_output_files(payload.node_count, "");

  for (int i = 0; i < payload.edge_count; ++i) {
    adj[payload.edges[i].from_id].push_back(payload.edges[i].to_id);
    in_degree[payload.edges[i].to_id]++;
  }

  std::vector<std::atomic<bool>> completed(payload.node_count);
  std::vector<std::atomic<bool>> running(payload.node_count);
  for (int i = 0; i < payload.node_count; ++i) {
    completed[i] = false;
    running[i] = false;
  }

  while (true) {
    std::vector<int> ready_nodes;
    for (int i = 0; i < payload.node_count; ++i) {
      if (in_degree[i] == 0 && !completed[i] && !running[i]) {
        ready_nodes.push_back(i);
      }
    }

    if (ready_nodes.empty()) {
      bool all_done = true;
      for (int i = 0; i < payload.node_count; ++i)
        if (!completed[i])
          all_done = false;
      if (all_done)
        break;
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      continue;
    }

    std::vector<std::jthread> threads;
    std::mutex socket_mutex;

    for (int node_id : ready_nodes) {
      running[node_id] = true;
      threads.emplace_back([&, node_id]() {
        const auto &node = payload.nodes[node_id];
        logger::info("Naming Server: Executing Node " +
                     std::to_string(node_id) + ": " + node.command_line);

        std::vector<commands::ServerDetails> available_js;
        {
          std::shared_lock lock(impl_->trie_mutex);
          for (const auto &[id, details] : impl_->job_servers) {
            if (details.online)
              available_js.push_back(details);
          }
        }

        if (available_js.empty()) {
          logger::error("Naming Server: No Job Servers for node " +
                        std::to_string(node_id));
          std::lock_guard lock(socket_mutex);
          commands::FilePacket error_packet;
          std::string msg = "Error: No Job Servers for node " +
                            std::to_string(node_id) + "\n";
          std::memcpy(error_packet.chunk, msg.c_str(), msg.size());
          error_packet.size = msg.size();
          (void)network::send_all(client_socket, &error_packet,
                                  sizeof(error_packet));
          completed[node_id] = true;
          return;
        }

        auto selected_js = lb::LoadBalancer::select_js(available_js);
        auto js_sock_res =
            network::connect_to_server(selected_js.ip, selected_js.port_nm);
        if (!js_sock_res) {
          logger::error("Naming Server: Failed to connect to JS for node " +
                        std::to_string(node_id));
          completed[node_id] = true;
          return;
        }

        commands::ClientRequest js_req{};
        js_req.command = commands::Command::EXECUTE_TASK;
        commands::JobPayload js_payload{};
        std::strncpy(js_payload.command_line, node.command_line,
                     sizeof(js_payload.command_line) - 1);

        for (int i = 0; i < payload.edge_count; ++i) {
          if (payload.edges[i].to_id == node_id &&
              payload.edges[i].type == commands::EdgeType::PIPE_DATA) {
            std::strncpy(js_payload.target_file,
                         node_output_files[payload.edges[i].from_id].c_str(),
                         sizeof(js_payload.target_file) - 1);
            logger::info("Naming Server: Node " + std::to_string(node_id) +
                         " reading from pipe: " + js_payload.target_file);
            break;
          }
        }

        if (js_payload.target_file[0] == '\0' && node.target_file[0] != '\0') {
          std::strncpy(js_payload.target_file, node.target_file,
                       sizeof(js_payload.target_file) - 1);
          logger::info("Naming Server: Node " + std::to_string(node_id) +
                       " reading from file: " + js_payload.target_file);
        }

        (void)network::send_all(*js_sock_res, &js_req, sizeof(js_req));
        (void)network::send_all(*js_sock_res, &js_payload, sizeof(js_payload));

        commands::AckPacket js_ack;
        if (!network::receive_all(*js_sock_res, &js_ack, sizeof(js_ack)) ||
            js_ack.status != commands::Status::Success) {
          logger::error("Naming Server: JS failed to start node " +
                        std::to_string(node_id));
          completed[node_id] = true;
          return;
        }

        bool is_pipe_source = false;
        for (int i = 0; i < payload.edge_count; ++i) {
          if (payload.edges[i].from_id == node_id &&
              payload.edges[i].type == commands::EdgeType::PIPE_DATA) {
            is_pipe_source = true;
            break;
          }
        }

        std::string temp_path = "";
        std::optional<network::Socket> ss_sock;
        if (is_pipe_source) {
          temp_path = "/tmp/dag_pipe_" + std::to_string(node_id) + ".tmp";
          node_output_files[node_id] = temp_path;
          logger::info("Naming Server: Node " + std::to_string(node_id) +
                       " writing to pipe: " + temp_path);

          auto online_ss = get_online_server_ids();
          if (!online_ss.empty()) {
            register_path(temp_path, {online_ss[0]}, true);
            auto ss_details = get_server_details(online_ss[0]);
            if (ss_details) {
              auto res = network::connect_to_server(ss_details->ip,
                                                    ss_details->port_client);
              if (res) {
                ss_sock = std::move(*res);
                commands::ClientRequest write_req{};
                write_req.command = commands::Command::WRITE_FILE;
                std::strncpy(write_req.arg1, temp_path.c_str(),
                             sizeof(write_req.arg1) - 1);
                (void)network::send_all(*ss_sock, &write_req,
                                        sizeof(write_req));
              }
            }
          }
        }

        commands::FilePacket packet;
        while (true) {
          auto r = network::receive_all(*js_sock_res, &packet, sizeof(packet));
          if (!r)
            break;

          if (ss_sock && packet.size > 0) {
            (void)network::send_all(*ss_sock, &packet, sizeof(packet));
          }

          if (!is_pipe_source && packet.size > 0) {
            packet.is_last = false;
            std::lock_guard lock(socket_mutex);
            (void)network::send_all(client_socket, &packet, sizeof(packet));
          }
          if (packet.size == 0)
            break;
        }

        if (ss_sock) {
          commands::FilePacket last_p{.size = 0, .is_last = true};
          (void)network::send_all(*ss_sock, &last_p, sizeof(last_p));

          // Sync: Read Ack from SS
          commands::AckPacket write_ack;
          (void)network::receive_all(*ss_sock, &write_ack, sizeof(write_ack));
          logger::info("Naming Server: Node " + std::to_string(node_id) +
                       " pipe sync complete.");
        }

        completed[node_id] = true;
        logger::info("Naming Server: Node " + std::to_string(node_id) +
                     " completed.");
      });
    }

    threads.clear();
    for (int node_id : ready_nodes) {
      for (int neighbor : adj[node_id]) {
        in_degree[neighbor]--;
      }
    }
  }

  commands::FilePacket final_packet;
  final_packet.size = 0;
  final_packet.is_last = true;
  (void)network::send_all(client_socket, &final_packet, sizeof(final_packet));
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
}

} // namespace naming
