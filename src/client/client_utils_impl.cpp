module;
#include <algorithm>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

module client;
import commands;

namespace client::utils {

commands::DAGPayload parse_dag(std::string_view input, std::string_view cwd) {
  commands::DAGPayload payload{};

  struct Token {
    std::string content;
    enum Type { CMD, PIPE, SEQ, ASYNC } type;
  };
  std::vector<Token> tokens;

  std::string current;
  for (size_t i = 0; i < input.size(); ++i) {
    if (input[i] == '|') {
      if (!current.empty())
        tokens.push_back({current, Token::CMD});
      tokens.push_back({"|", Token::PIPE});
      current.clear();
    } else if (input[i] == ';') {
      if (!current.empty())
        tokens.push_back({current, Token::CMD});
      tokens.push_back({";", Token::SEQ});
      current.clear();
    } else if (input[i] == '&') {
      if (i + 1 < input.size() && input[i + 1] == '&') {
        if (!current.empty())
          tokens.push_back({current, Token::CMD});
        tokens.push_back({"&&", Token::SEQ});
        i++;
      } else {
        if (!current.empty())
          tokens.push_back({current, Token::CMD});
        tokens.push_back({"&", Token::ASYNC});
      }
      current.clear();
    } else if (input[i] == '(' || input[i] == ')') {
      if (!current.empty())
        tokens.push_back({current, Token::CMD});
      current.clear();
    } else {
      current += input[i];
    }
  }
  if (!current.empty())
    tokens.push_back({current, Token::CMD});

  for (auto &t : tokens) {
    if (t.type == Token::CMD) {
      t.content.erase(0, t.content.find_first_not_of(" \t\n\r"));
      t.content.erase(t.content.find_last_not_of(" \t\n\r") + 1);

      if (t.content.starts_with("job ")) {
        t.content.erase(0, 4);
        t.content.erase(0, t.content.find_first_not_of(" \t\n\r"));
      }
    }
  }
  tokens.erase(std::remove_if(tokens.begin(), tokens.end(),
                              [](const Token &t) {
                                return t.type == Token::CMD &&
                                       t.content.empty();
                              }),
               tokens.end());

  if (!tokens.empty() && tokens.back().type == Token::PIPE) {
    return payload; // Returns payload with node_count = 0
  }

  int last_node_id = -1;
  bool next_is_parallel = false;

  for (size_t i = 0; i < tokens.size(); ++i) {
    if (tokens[i].type == Token::CMD) {
      int node_id = payload.node_count++;
      payload.nodes[node_id].id = node_id;
      payload.nodes[node_id].type = commands::NodeType::COMMAND;

      bool is_pipe_dest = (i > 0 && tokens[i - 1].type == Token::PIPE);
      auto args = split_args(tokens[i].content);

      if (is_pipe_dest) {
        // For pipe destinations, the whole content is the command line.
        std::strncpy(payload.nodes[node_id].command_line,
                     tokens[i].content.c_str(),
                     sizeof(payload.nodes[node_id].command_line) - 1);
        payload.nodes[node_id].target_file[0] = '\0';
      } else if (!args.empty()) {
        // Smart stripping: only if last arg looks like a file/path
        std::string last = args.back();
        bool looks_like_path = last.find('/') != std::string::npos ||
                               last.find('.') != std::string::npos;

        if (args.size() > 1 && looks_like_path) {
          std::string resolved = resolve_path(cwd, last);
          std::strncpy(payload.nodes[node_id].target_file, resolved.c_str(),
                       sizeof(payload.nodes[node_id].target_file) - 1);

          std::string stripped_cmd;
          for (size_t j = 0; j < args.size() - 1; ++j) {
            if (args[j].find(' ') != std::string::npos) {
              stripped_cmd += "\"" + args[j] + "\"";
            } else {
              stripped_cmd += args[j];
            }
            if (j < args.size() - 2)
              stripped_cmd += " ";
          }
          std::strncpy(payload.nodes[node_id].command_line,
                       stripped_cmd.c_str(),
                       sizeof(payload.nodes[node_id].command_line) - 1);
        } else {
          // No target file, use full command line
          std::strncpy(payload.nodes[node_id].command_line,
                       tokens[i].content.c_str(),
                       sizeof(payload.nodes[node_id].command_line) - 1);
          payload.nodes[node_id].target_file[0] = '\0';
        }
      }

      if (last_node_id != -1 && !next_is_parallel) {
        commands::EdgeType edge_type = commands::EdgeType::CONTROL_SEQ;
        if (i > 0 && tokens[i - 1].type == Token::PIPE) {
          edge_type = commands::EdgeType::PIPE_DATA;
        }

        int edge_id = payload.edge_count++;
        payload.edges[edge_id].from_id = last_node_id;
        payload.edges[edge_id].to_id = node_id;
        payload.edges[edge_id].type = edge_type;
      }

      last_node_id = node_id;
      next_is_parallel = false;
    } else if (tokens[i].type == Token::ASYNC) {
      next_is_parallel = true;
      last_node_id = -1;
    } else if (tokens[i].type == Token::SEQ) {
      next_is_parallel = false;
    }
  }

  return payload;
}

} // namespace client::utils
