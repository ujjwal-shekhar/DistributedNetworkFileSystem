module;

#include <expected>
#include <string>

export module network;

import core;

export import :types;
export import :internal;

namespace network {
export std::expected<void, Error> send_all(const Socket &sock, const void *data,
                                           size_t size);
export std::expected<void, Error> receive_all(const Socket &sock, void *buffer,
                                              size_t size);
} // namespace network
