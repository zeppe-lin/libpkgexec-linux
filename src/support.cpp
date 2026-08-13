// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#include "support.h"

#include <libpkgexec-linux/error.h>

#include <openssl/evp.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <iomanip>
#include <memory>
#include <sstream>
#include <system_error>

#include <fcntl.h>
#include <grp.h>
#include <sys/stat.h>
#include <unistd.h>

namespace pkgexec_linux::detail {
namespace {

struct md_context_deleter {
  void operator()(EVP_MD_CTX* value) const noexcept { EVP_MD_CTX_free(value); }
};

std::string hex_bytes(const unsigned char* bytes, std::size_t size)
{
  std::ostringstream out;
  out << std::hex << std::setfill('0');
  for (std::size_t i = 0; i < size; ++i) {
    out << std::setw(2) << static_cast<unsigned int>(bytes[i]);
  }
  return out.str();
}

void update(EVP_MD_CTX* context, const void* data, std::size_t size)
{
  if (size != 0U && EVP_DigestUpdate(context, data, size) != 1) {
    throw error(error_code::interpreter_inspection_failed,
                "SHA-256 update failed");
  }
}

} // namespace

std::string sha256_hex(std::string_view domain, std::string_view material)
{
  std::unique_ptr<EVP_MD_CTX, md_context_deleter> context(EVP_MD_CTX_new());
  if (!context || EVP_DigestInit_ex(context.get(), EVP_sha256(), nullptr) != 1) {
    throw error(error_code::interpreter_inspection_failed,
                "SHA-256 initialization failed");
  }
  update(context.get(), domain.data(), domain.size());
  const unsigned char zero = 0;
  update(context.get(), &zero, 1);
  update(context.get(), material.data(), material.size());
  std::array<unsigned char, EVP_MAX_MD_SIZE> bytes{};
  unsigned int size = 0;
  if (EVP_DigestFinal_ex(context.get(), bytes.data(), &size) != 1 ||
      size != 32U) {
    throw error(error_code::interpreter_inspection_failed,
                "SHA-256 finalization failed");
  }
  return hex_bytes(bytes.data(), size);
}

pkgexec::sha256_digest digest_fd(int fd)
{
  std::unique_ptr<EVP_MD_CTX, md_context_deleter> context(EVP_MD_CTX_new());
  if (!context || EVP_DigestInit_ex(context.get(), EVP_sha256(), nullptr) != 1) {
    throw error(error_code::interpreter_inspection_failed,
                "SHA-256 initialization failed");
  }
  std::array<char, 64 * 1024> buffer{};
  off_t offset = 0;
  for (;;) {
    const ssize_t count = ::pread(fd, buffer.data(), buffer.size(), offset);
    if (count > 0) {
      update(context.get(), buffer.data(), static_cast<std::size_t>(count));
      offset += count;
      continue;
    }
    if (count == 0) {
      break;
    }
    if (errno == EINTR) {
      continue;
    }
    throw error(error_code::interpreter_inspection_failed,
                errno_message("pread interpreter", errno));
  }
  std::array<unsigned char, EVP_MAX_MD_SIZE> bytes{};
  unsigned int size = 0;
  if (EVP_DigestFinal_ex(context.get(), bytes.data(), &size) != 1 ||
      size != 32U) {
    throw error(error_code::interpreter_inspection_failed,
                "SHA-256 finalization failed");
  }
  return pkgexec::sha256_digest(hex_bytes(bytes.data(), size));
}

pkgexec::sha256_digest digest_file(const std::filesystem::path& path)
{
  const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
  if (fd < 0) {
    throw error(error_code::interpreter_inspection_failed,
                errno_message("open interpreter", errno));
  }
  try {
    struct stat info {};
    if (::fstat(fd, &info) != 0 || !S_ISREG(info.st_mode)) {
      throw error(error_code::interpreter_inspection_failed,
                  "interpreter descriptor is not a regular file");
    }
    auto result = digest_fd(fd);
    ::close(fd);
    return result;
  } catch (...) {
    ::close(fd);
    throw;
  }
}

pkgexec::interpreter_identity interpreter_identity(
    const pkgexec::sha256_digest& digest)
{
  return pkgexec::interpreter_identity::from_sha256(
      sha256_hex("pkgexec-linux/interpreter/v1", digest.hex()));
}

bool path_has_symlink_component(const std::filesystem::path& path)
{
  if (!path.is_absolute()) {
    return true;
  }
  std::filesystem::path current = path.root_path();
  for (const auto& component : path.relative_path()) {
    current /= component;
    struct stat info {};
    if (::lstat(current.c_str(), &info) != 0) {
      return true;
    }
    if (S_ISLNK(info.st_mode)) {
      return true;
    }
  }
  return false;
}

std::vector<std::uint64_t> current_groups()
{
  const int count = ::getgroups(0, nullptr);
  if (count < 0) {
    throw error(error_code::invalid_value,
                errno_message("getgroups", errno));
  }
  std::vector<gid_t> native(static_cast<std::size_t>(count));
  if (count > 0 && ::getgroups(count, native.data()) < 0) {
    throw error(error_code::invalid_value,
                errno_message("getgroups", errno));
  }
  std::vector<std::uint64_t> result;
  result.reserve(native.size());
  for (const auto value : native) {
    result.push_back(static_cast<std::uint64_t>(value));
  }
  std::sort(result.begin(), result.end());
  result.erase(std::unique(result.begin(), result.end()), result.end());
  result.erase(std::remove(result.begin(), result.end(),
                           static_cast<std::uint64_t>(::getgid())),
               result.end());
  return result;
}

std::string errno_message(std::string_view operation, int value)
{
  return std::string(operation) + ": " + std::strerror(value);
}

} // namespace pkgexec_linux::detail
