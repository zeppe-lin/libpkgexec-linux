// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

#include <arpa/inet.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace test_support {

class parent_listener final {
public:
  parent_listener()
  {
    socket_ = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    if (socket_ < 0) {
      throw std::runtime_error("cannot create parent network listener");
    }
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    if (::bind(socket_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 ||
        ::listen(socket_, 4) != 0) {
      const int saved = errno;
      ::close(socket_);
      socket_ = -1;
      throw std::runtime_error("cannot bind parent network listener: " +
                               std::to_string(saved));
    }
    socklen_t size = sizeof(address);
    if (::getsockname(socket_, reinterpret_cast<sockaddr*>(&address), &size) != 0) {
      ::close(socket_);
      socket_ = -1;
      throw std::runtime_error("cannot inspect parent network listener");
    }
    port_ = ntohs(address.sin_port);
  }

  ~parent_listener()
  {
    if (socket_ >= 0) {
      ::close(socket_);
    }
  }

  [[nodiscard]] std::uint16_t port() const noexcept { return port_; }

  [[nodiscard]] bool received(int timeout_milliseconds)
  {
    pollfd descriptor{socket_, POLLIN, 0};
    const int ready = ::poll(&descriptor, 1, timeout_milliseconds);
    if (ready <= 0 || (descriptor.revents & POLLIN) == 0) {
      return false;
    }
    const int accepted = ::accept4(socket_, nullptr, nullptr, SOCK_CLOEXEC);
    if (accepted < 0) {
      return false;
    }
    char value = 0;
    const bool okay = ::recv(accepted, &value, sizeof(value), MSG_WAITALL) ==
                          sizeof(value) &&
                      value == 'p';
    ::close(accepted);
    return okay;
  }

private:
  int socket_ = -1;
  std::uint16_t port_ = 0;
};

} // namespace test_support
