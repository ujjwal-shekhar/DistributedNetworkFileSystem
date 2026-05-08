module;

#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>

export module storage_server:sync;

namespace storage {

// FEEDBACK: We should surely have more docstring comments
// at all other places too, low priority though for now.

/**
 * @brief Provides fine-grained reader-writer locks for file paths.
 */
export class LockManager {
public:
  void acquire_read(const std::string &path) { get_mutex(path).lock_shared(); }

  void release_read(const std::string &path) {
    get_mutex(path).unlock_shared();
  }

  void acquire_write(const std::string &path) { get_mutex(path).lock(); }

  void release_write(const std::string &path) { get_mutex(path).unlock(); }

private:
  std::shared_mutex &get_mutex(const std::string &path) {
    std::lock_guard lock(map_mutex_);
    return mutexes_[path];
  }

  std::unordered_map<std::string, std::shared_mutex> mutexes_;
  std::mutex map_mutex_;
};

} // namespace storage
