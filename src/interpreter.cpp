// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#include <libpkgexec-linux/interpreter.h>

#include <libpkgexec-linux/error.h>

#include "support.h"

#include <tuple>
#include <utility>

#include <sys/stat.h>
#include <unistd.h>

namespace pkgexec_linux {

interpreter_binding::interpreter_binding(
    pkgexec::interpreter_identity identity,
    std::filesystem::path executable,
    pkgexec::sha256_digest content_digest)
    : identity_(std::move(identity)), executable_(std::move(executable)),
      content_digest_(std::move(content_digest))
{
}

interpreter_binding interpreter_binding::inspect(
    const std::filesystem::path& executable)
{
  if (!executable.is_absolute()) {
    throw error(error_code::invalid_value,
                "interpreter path must be absolute");
  }
  std::error_code ec;
  const auto canonical = std::filesystem::canonical(executable, ec);
  if (ec) {
    throw error(error_code::interpreter_inspection_failed,
                "cannot resolve interpreter " + executable.string() +
                ": " + ec.message());
  }
  if (detail::path_has_symlink_component(canonical)) {
    throw error(error_code::interpreter_inspection_failed,
                "resolved interpreter path contains a symlink component");
  }
  struct stat info {};
  if (::stat(canonical.c_str(), &info) != 0 || !S_ISREG(info.st_mode)) {
    throw error(error_code::interpreter_inspection_failed,
                "interpreter is not a regular file: " + canonical.string());
  }
  if (::access(canonical.c_str(), X_OK) != 0) {
    throw error(error_code::interpreter_inspection_failed,
                "interpreter is not executable: " + canonical.string());
  }
  auto digest = detail::digest_file(canonical);
  auto identity = detail::interpreter_identity(digest);
  return interpreter_binding(std::move(identity), canonical, std::move(digest));
}

const pkgexec::interpreter_identity& interpreter_binding::identity() const noexcept
{ return identity_; }
const std::filesystem::path& interpreter_binding::executable() const noexcept
{ return executable_; }
const pkgexec::sha256_digest& interpreter_binding::content_digest() const noexcept
{ return content_digest_; }

bool operator==(const interpreter_binding& lhs,
                const interpreter_binding& rhs) noexcept
{
  return lhs.identity_ == rhs.identity_ &&
         lhs.executable_ == rhs.executable_ &&
         lhs.content_digest_ == rhs.content_digest_;
}
bool operator!=(const interpreter_binding& lhs,
                const interpreter_binding& rhs) noexcept
{ return !(lhs == rhs); }
bool operator<(const interpreter_binding& lhs,
               const interpreter_binding& rhs) noexcept
{
  return std::tie(lhs.identity_, lhs.executable_, lhs.content_digest_) <
         std::tie(rhs.identity_, rhs.executable_, rhs.content_digest_);
}

} // namespace pkgexec_linux
