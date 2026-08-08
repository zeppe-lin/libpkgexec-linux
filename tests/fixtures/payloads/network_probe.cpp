// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string_view>

#include <arpa/inet.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {

class fd final {
public:
  explicit fd(int value = -1) noexcept : value_(value) {}
  ~fd()
  {
    if (value_ >= 0) {
      ::close(value_);
    }
  }
  fd(const fd&) = delete;
  fd& operator=(const fd&) = delete;
  [[nodiscard]] int get() const noexcept { return value_; }
  [[nodiscard]] explicit operator bool() const noexcept { return value_ >= 0; }
private:
  int value_;
};

struct link_snapshot final {
  std::uint32_t count = 0;
  std::uint32_t loopback_count = 0;
  unsigned int loopback_flags = 0;
};

bool valid_message(const nlmsghdr* header, int remaining) noexcept
{
  return remaining >= static_cast<int>(sizeof(nlmsghdr)) &&
         header->nlmsg_len >= sizeof(nlmsghdr) &&
         header->nlmsg_len <= static_cast<unsigned int>(remaining);
}

nlmsghdr* next_message(nlmsghdr* header, int& remaining) noexcept
{
  const int aligned = static_cast<int>(NLMSG_ALIGN(header->nlmsg_len));
  remaining -= aligned;
  return reinterpret_cast<nlmsghdr*>(
      reinterpret_cast<unsigned char*>(header) + aligned);
}

bool inspect_links(link_snapshot& snapshot)
{
  fd route(::socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_ROUTE));
  if (!route) {
    return false;
  }
  sockaddr_nl local{};
  local.nl_family = AF_NETLINK;
  if (::bind(route.get(), reinterpret_cast<sockaddr*>(&local),
             sizeof(local)) != 0) {
    return false;
  }
  struct request final {
    nlmsghdr header;
    ifinfomsg link;
  } request{};
  request.header.nlmsg_len = NLMSG_LENGTH(sizeof(ifinfomsg));
  request.header.nlmsg_type = RTM_GETLINK;
  request.header.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
  request.header.nlmsg_seq = 1U;
  request.link.ifi_family = AF_UNSPEC;
  sockaddr_nl kernel{};
  kernel.nl_family = AF_NETLINK;
  if (::sendto(route.get(), &request, request.header.nlmsg_len, 0,
               reinterpret_cast<sockaddr*>(&kernel), sizeof(kernel)) !=
      static_cast<ssize_t>(request.header.nlmsg_len)) {
    return false;
  }

  snapshot = {};
  std::array<unsigned char, 16384> buffer{};
  while (true) {
    const ssize_t received = ::recv(route.get(), buffer.data(), buffer.size(), 0);
    if (received < 0 && errno == EINTR) {
      continue;
    }
    if (received <= 0) {
      return false;
    }
    int remaining = static_cast<int>(received);
    for (auto* header = reinterpret_cast<nlmsghdr*>(buffer.data());
         valid_message(header, remaining);
         header = next_message(header, remaining)) {
      if (header->nlmsg_seq != 1U) {
        continue;
      }
      if (header->nlmsg_type == NLMSG_DONE) {
        return true;
      }
      if (header->nlmsg_type == NLMSG_ERROR) {
        return false;
      }
      if (header->nlmsg_type != RTM_NEWLINK ||
          header->nlmsg_len < NLMSG_LENGTH(sizeof(ifinfomsg))) {
        continue;
      }
      const auto* link =
          reinterpret_cast<const ifinfomsg*>(NLMSG_DATA(header));
      ++snapshot.count;
      if ((link->ifi_flags & IFF_LOOPBACK) != 0U) {
        ++snapshot.loopback_count;
        snapshot.loopback_flags = link->ifi_flags;
      }
    }
  }
}

bool internal_roundtrip()
{
  fd listener(::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0));
  if (!listener) {
    return false;
  }
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  address.sin_port = 0;
  if (::bind(listener.get(), reinterpret_cast<sockaddr*>(&address),
             sizeof(address)) != 0 ||
      ::listen(listener.get(), 1) != 0) {
    return false;
  }
  socklen_t size = sizeof(address);
  if (::getsockname(listener.get(), reinterpret_cast<sockaddr*>(&address),
                    &size) != 0) {
    return false;
  }
  fd client(::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0));
  if (!client ||
      ::connect(client.get(), reinterpret_cast<sockaddr*>(&address),
                sizeof(address)) != 0) {
    return false;
  }
  fd accepted(::accept4(listener.get(), nullptr, nullptr, SOCK_CLOEXEC));
  if (!accepted) {
    return false;
  }
  constexpr char value = 'x';
  char observed = 0;
  return ::send(client.get(), &value, sizeof(value), MSG_NOSIGNAL) ==
             sizeof(value) &&
         ::recv(accepted.get(), &observed, sizeof(observed), MSG_WAITALL) ==
             sizeof(observed) &&
         observed == value;
}

bool connect_parent(std::uint16_t port)
{
  fd client(::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0));
  if (!client) {
    return false;
  }
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  address.sin_port = htons(port);
  if (::connect(client.get(), reinterpret_cast<sockaddr*>(&address),
                sizeof(address)) != 0) {
    return false;
  }
  constexpr char value = 'p';
  return ::send(client.get(), &value, sizeof(value), MSG_NOSIGNAL) ==
         sizeof(value);
}

std::uint16_t parse_port(const char* value)
{
  char* end = nullptr;
  const unsigned long parsed = std::strtoul(value, &end, 10);
  if (!end || *end != '\0' || parsed == 0UL || parsed > 65535UL) {
    return 0;
  }
  return static_cast<std::uint16_t>(parsed);
}

int run(std::string_view mode, std::uint16_t parent_port)
{
  if (mode == "allowed") {
    return connect_parent(parent_port) ? 0 : 1;
  }

  link_snapshot links{};
  if (!inspect_links(links) || links.count != 1U ||
      links.loopback_count != 1U) {
    std::cerr << "unexpected isolated network link view\n";
    return 1;
  }
  const bool loopback_up = (links.loopback_flags & IFF_UP) != 0U;
  if (mode == "denied") {
    if (loopback_up || connect_parent(parent_port)) {
      std::cerr << "denied network view remains usable\n";
      return 1;
    }
    return 0;
  }
  if (mode == "loopback") {
    if (!loopback_up || !internal_roundtrip() || connect_parent(parent_port)) {
      std::cerr << "loopback-only network view is incorrect\n";
      return 1;
    }
    return 0;
  }
  return 2;
}

} // namespace

int main(int argc, char** argv)
{
  if (argc != 3) {
    return 2;
  }
  const auto port = parse_port(argv[2]);
  return port == 0 ? 2 : run(argv[1], port);
}
