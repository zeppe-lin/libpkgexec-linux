// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#include "network_isolation.h"

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstring>

#include <arpa/inet.h>
#include <fcntl.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#include <sched.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace pkgexec_linux::detail {
namespace {

class local_fd final {
public:
  local_fd() noexcept = default;
  explicit local_fd(int value) noexcept : value_(value) {}
  ~local_fd()
  {
    if (value_ >= 0) {
      ::close(value_);
    }
  }
  local_fd(const local_fd&) = delete;
  local_fd& operator=(const local_fd&) = delete;
  local_fd(local_fd&& other) noexcept : value_(other.release()) {}
  local_fd& operator=(local_fd&& other) noexcept
  {
    if (this != &other) {
      if (value_ >= 0) {
        ::close(value_);
      }
      value_ = other.release();
    }
    return *this;
  }
  [[nodiscard]] int get() const noexcept { return value_; }
  [[nodiscard]] explicit operator bool() const noexcept { return value_ >= 0; }
private:
  int release() noexcept
  {
    const int value = value_;
    value_ = -1;
    return value;
  }
  int value_ = -1;
};

struct link_snapshot final {
  std::uint32_t count = 0;
  std::uint32_t loopback_count = 0;
  int loopback_index = 0;
  unsigned int loopback_flags = 0;
};

bool valid_netlink_message(const nlmsghdr* header, int remaining) noexcept
{
  return remaining >= static_cast<int>(sizeof(nlmsghdr)) &&
         header->nlmsg_len >= sizeof(nlmsghdr) &&
         header->nlmsg_len <= static_cast<unsigned int>(remaining);
}

nlmsghdr* next_netlink_message(nlmsghdr* header, int& remaining) noexcept
{
  const int aligned = static_cast<int>(NLMSG_ALIGN(header->nlmsg_len));
  remaining -= aligned;
  return reinterpret_cast<nlmsghdr*>(
      reinterpret_cast<unsigned char*>(header) + aligned);
}

local_fd open_route_socket() noexcept
{
  local_fd socket_fd(::socket(
      AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_ROUTE));
  if (!socket_fd) {
    return {};
  }
  sockaddr_nl address{};
  address.nl_family = AF_NETLINK;
  if (::bind(socket_fd.get(), reinterpret_cast<sockaddr*>(&address),
             sizeof(address)) != 0) {
    return {};
  }
  return socket_fd;
}

bool send_netlink(int fd, const void* message, std::size_t size) noexcept
{
  sockaddr_nl kernel{};
  kernel.nl_family = AF_NETLINK;
  while (true) {
    const ssize_t count = ::sendto(
        fd, message, size, 0, reinterpret_cast<sockaddr*>(&kernel),
        sizeof(kernel));
    if (count == static_cast<ssize_t>(size)) {
      return true;
    }
    if (count < 0 && errno == EINTR) {
      continue;
    }
    if (count >= 0) {
      errno = EIO;
    }
    return false;
  }
}

bool receive_ack(int fd, std::uint32_t sequence) noexcept
{
  std::array<unsigned char, 8192> buffer{};
  while (true) {
    const ssize_t received = ::recv(fd, buffer.data(), buffer.size(), 0);
    if (received < 0 && errno == EINTR) {
      continue;
    }
    if (received <= 0) {
      if (received == 0) {
        errno = EIO;
      }
      return false;
    }
    int remaining = static_cast<int>(received);
    for (auto* header = reinterpret_cast<nlmsghdr*>(buffer.data());
         valid_netlink_message(header, remaining);
         header = next_netlink_message(header, remaining)) {
      if (header->nlmsg_seq != sequence) {
        continue;
      }
      if (header->nlmsg_type != NLMSG_ERROR) {
        continue;
      }
      if (header->nlmsg_len < NLMSG_LENGTH(sizeof(nlmsgerr))) {
        errno = EIO;
        return false;
      }
      const auto* response = reinterpret_cast<const nlmsgerr*>(NLMSG_DATA(header));
      if (response->error == 0) {
        return true;
      }
      errno = -response->error;
      return false;
    }
  }
}

bool inspect_links(int fd, link_snapshot& snapshot) noexcept
{
  struct request final {
    nlmsghdr header;
    ifinfomsg link;
  } value{};
  constexpr std::uint32_t sequence = 1U;
  value.header.nlmsg_len = NLMSG_LENGTH(sizeof(ifinfomsg));
  value.header.nlmsg_type = RTM_GETLINK;
  value.header.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
  value.header.nlmsg_seq = sequence;
  value.link.ifi_family = AF_UNSPEC;
  if (!send_netlink(fd, &value, value.header.nlmsg_len)) {
    return false;
  }

  snapshot = {};
  std::array<unsigned char, 16384> buffer{};
  while (true) {
    const ssize_t received = ::recv(fd, buffer.data(), buffer.size(), 0);
    if (received < 0 && errno == EINTR) {
      continue;
    }
    if (received <= 0) {
      if (received == 0) {
        errno = EIO;
      }
      return false;
    }
    int remaining = static_cast<int>(received);
    for (auto* header = reinterpret_cast<nlmsghdr*>(buffer.data());
         valid_netlink_message(header, remaining);
         header = next_netlink_message(header, remaining)) {
      if (header->nlmsg_seq != sequence) {
        continue;
      }
      if (header->nlmsg_type == NLMSG_DONE) {
        return true;
      }
      if (header->nlmsg_type == NLMSG_ERROR) {
        if (header->nlmsg_len < NLMSG_LENGTH(sizeof(nlmsgerr))) {
          errno = EIO;
          return false;
        }
        const auto* response =
            reinterpret_cast<const nlmsgerr*>(NLMSG_DATA(header));
        errno = response->error == 0 ? EIO : -response->error;
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
        snapshot.loopback_index = link->ifi_index;
        snapshot.loopback_flags = link->ifi_flags;
      }
    }
  }
}

bool set_link_up(int fd, int index, bool up) noexcept
{
  struct request final {
    nlmsghdr header;
    ifinfomsg link;
  } value{};
  constexpr std::uint32_t sequence = 2U;
  value.header.nlmsg_len = NLMSG_LENGTH(sizeof(ifinfomsg));
  value.header.nlmsg_type = RTM_NEWLINK;
  value.header.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
  value.header.nlmsg_seq = sequence;
  value.link.ifi_family = AF_UNSPEC;
  value.link.ifi_index = index;
  value.link.ifi_flags = up ? static_cast<unsigned int>(IFF_UP) : 0U;
  value.link.ifi_change = IFF_UP;
  return send_netlink(fd, &value, value.header.nlmsg_len) &&
         receive_ack(fd, sequence);
}

bool snapshot_matches(const link_snapshot& snapshot,
                      pkgexec::network_policy policy) noexcept
{
  if (snapshot.count != 1U || snapshot.loopback_count != 1U ||
      snapshot.loopback_index <= 0) {
    return false;
  }
  const bool loopback_up = (snapshot.loopback_flags & IFF_UP) != 0U;
  return policy == pkgexec::network_policy::loopback_only
      ? loopback_up
      : !loopback_up;
}

bool loopback_roundtrip() noexcept
{
  local_fd listener(::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0));
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
  socklen_t address_size = sizeof(address);
  if (::getsockname(listener.get(), reinterpret_cast<sockaddr*>(&address),
                    &address_size) != 0) {
    return false;
  }

  local_fd client(::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0));
  if (!client ||
      ::connect(client.get(), reinterpret_cast<sockaddr*>(&address),
                sizeof(address)) != 0) {
    return false;
  }
  local_fd accepted(::accept4(listener.get(), nullptr, nullptr, SOCK_CLOEXEC));
  if (!accepted) {
    return false;
  }
  constexpr char sent = 'n';
  char received = 0;
  if (::send(client.get(), &sent, sizeof(sent), MSG_NOSIGNAL) != sizeof(sent) ||
      ::recv(accepted.get(), &received, sizeof(received), MSG_WAITALL) !=
          sizeof(received) ||
      received != sent) {
    errno = EIO;
    return false;
  }
  return true;
}

void write_failure(int fd, const network_setup_failure& failure) noexcept
{
  const auto* bytes = reinterpret_cast<const unsigned char*>(&failure);
  std::size_t offset = 0;
  while (offset < sizeof(failure)) {
    const ssize_t count = ::write(fd, bytes + offset, sizeof(failure) - offset);
    if (count > 0) {
      offset += static_cast<std::size_t>(count);
      continue;
    }
    if (count < 0 && errno == EINTR) {
      continue;
    }
    return;
  }
}

} // namespace

bool setup_network_policy(pkgexec::network_policy policy,
                          network_setup_failure& failure) noexcept
{
  if (policy == pkgexec::network_policy::allowed) {
    return true;
  }
  if (::unshare(CLONE_NEWNET) != 0) {
    failure = {network_setup_stage::network_namespace, errno};
    return false;
  }

  auto route = open_route_socket();
  if (!route) {
    failure = {network_setup_stage::link_inspection, errno};
    return false;
  }
  link_snapshot initial{};
  if (!inspect_links(route.get(), initial)) {
    failure = {network_setup_stage::link_inspection, errno};
    return false;
  }
  if (initial.count != 1U || initial.loopback_count != 1U ||
      initial.loopback_index <= 0) {
    failure = {network_setup_stage::policy_validation, ENODEV};
    return false;
  }
  const bool up = policy == pkgexec::network_policy::loopback_only;
  if (!set_link_up(route.get(), initial.loopback_index, up)) {
    failure = {network_setup_stage::link_configuration, errno};
    return false;
  }
  link_snapshot established{};
  if (!inspect_links(route.get(), established)) {
    failure = {network_setup_stage::link_inspection, errno};
    return false;
  }
  if (!snapshot_matches(established, policy)) {
    failure = {network_setup_stage::policy_validation, EIO};
    return false;
  }
  return true;
}

bool probe_network_policy(pkgexec::network_policy policy,
                          network_setup_failure& failure) noexcept
{
  if (policy == pkgexec::network_policy::allowed) {
    return true;
  }
  int report[2] = {-1, -1};
  if (::pipe2(report, O_CLOEXEC) != 0) {
    failure = {network_setup_stage::network_namespace, errno};
    return false;
  }
  const pid_t child = ::fork();
  if (child < 0) {
    failure = {network_setup_stage::network_namespace, errno};
    ::close(report[0]);
    ::close(report[1]);
    return false;
  }
  if (child == 0) {
    ::close(report[0]);
    network_setup_failure child_failure{};
    if (!setup_network_policy(policy, child_failure)) {
      write_failure(report[1], child_failure);
      _exit(1);
    }
    if (policy == pkgexec::network_policy::loopback_only &&
        !loopback_roundtrip()) {
      child_failure = {network_setup_stage::loopback_roundtrip,
                       errno == 0 ? EIO : errno};
      write_failure(report[1], child_failure);
      _exit(1);
    }
    ::close(report[1]);
    _exit(0);
  }

  ::close(report[1]);
  std::array<unsigned char, sizeof(network_setup_failure)> bytes{};
  std::size_t size = 0;
  while (size < bytes.size()) {
    const ssize_t count = ::read(report[0], bytes.data() + size,
                                 bytes.size() - size);
    if (count > 0) {
      size += static_cast<std::size_t>(count);
      continue;
    }
    if (count < 0 && errno == EINTR) {
      continue;
    }
    break;
  }
  ::close(report[0]);
  int status = 0;
  pid_t waited = -1;
  do {
    waited = ::waitpid(child, &status, 0);
  } while (waited < 0 && errno == EINTR);
  if (waited != child) {
    failure = {network_setup_stage::policy_validation,
               errno == 0 ? ECHILD : errno};
    return false;
  }
  if (WIFEXITED(status) && WEXITSTATUS(status) == 0 && size == 0U) {
    return true;
  }
  if (size == sizeof(failure)) {
    std::memcpy(&failure, bytes.data(), sizeof(failure));
  } else {
    failure = {network_setup_stage::policy_validation, EIO};
  }
  return false;
}

std::string_view network_stage_name(network_setup_stage stage) noexcept
{
  switch (stage) {
    case network_setup_stage::network_namespace: return "network namespace";
    case network_setup_stage::link_inspection: return "network link inspection";
    case network_setup_stage::link_configuration: return "loopback configuration";
    case network_setup_stage::policy_validation: return "network policy validation";
    case network_setup_stage::loopback_roundtrip: return "loopback round trip";
  }
  return "network isolation";
}

} // namespace pkgexec_linux::detail
