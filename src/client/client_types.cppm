module;

#include <string>

export module client:types;

export namespace client {

enum class Error {
  ConnectionFailed,
  InvalidCommand,
  TransferFailed,
  AuthenticationRequired,
  InsufficientTokens
};

struct Session {
  std::string client_id;
  int remaining_tokens = 0;
  bool is_admin = false;
};

} // namespace client
