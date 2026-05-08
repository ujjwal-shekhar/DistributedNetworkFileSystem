export module naming_server:types;

namespace naming {

export enum class Error {
  PathNotFound,
  InvalidPath,
  OperationFailed,
  InsufficientTokens,
  AuthenticationFailed,
  NetworkError
};

}
