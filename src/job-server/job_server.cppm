module;

#include <expected>
#include <memory>
#include <string_view>

export module job_server;

export namespace job_server {

enum class Error {
  ConnectionFailed,
  RegistrationFailed,
  OperationFailed,
  NetworkError
};

class JobServer {
public:
  JobServer();
  ~JobServer();

  JobServer(const JobServer &) = delete;
  JobServer &operator=(const JobServer &) = delete;

  [[nodiscard]] std::expected<void, Error>
  run(std::string_view nm_ip, int nm_reg_port, int nm_clt_port);

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace job_server
