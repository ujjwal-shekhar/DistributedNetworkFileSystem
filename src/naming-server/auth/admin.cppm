module;

#include <string_view>

export module naming_server:admin;

import :commands;

namespace naming {

export class AdminService {
public:
  [[nodiscard]] static bool authenticate(std::string_view password) {
    return password == "admin_secret_2026";
  }

  [[nodiscard]] static bool is_admin_command(Command cmd) {
    return get_metadata(cmd).tier == PrivilegeTier::ADMIN;
  }

  [[nodiscard]] static bool is_privileged_command(Command cmd) {
    return get_metadata(cmd).tier == PrivilegeTier::PRIVILEGED;
  }
};

} // namespace naming
