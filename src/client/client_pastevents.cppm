module;

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

export module client:pastevents;

namespace client::pastevents {

export class History {
public:
  explicit History(size_t max_size) : max_size_(max_size) { load(); }

  void add(std::string_view command) {
    if (command.empty() || command.find("pastevents") != std::string_view::npos)
      return;

    if (!history_.empty() && history_.back() == command)
      return;

    history_.emplace_back(command);
    if (history_.size() > max_size_) {
      history_.erase(history_.begin());
    }
    save();
  }

  const std::vector<std::string> &get_all() const { return history_; }

  void purge() {
    history_.clear();
    save();
  }

  std::string get_by_index(int index) const {
    if (index <= 0 || static_cast<size_t>(index) > history_.size())
      return "";
    return history_[history_.size() - index];
  }

private:
  void load() {
    if (!std::filesystem::exists(".pastevents.log"))
      return;
    std::ifstream file(".pastevents.log");
    std::string line;
    while (std::getline(file, line)) {
      if (!line.empty())
        history_.push_back(line);
    }
  }

  void save() {
    std::ofstream file(".pastevents.log", std::ios::trunc);
    for (const auto &cmd : history_) {
      file << cmd << "\n";
    }
  }

  std::vector<std::string> history_;
  size_t max_size_;
};

} // namespace client::pastevents
