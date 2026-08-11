// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file interpreter.h
 *  \brief Exact inspected interpreter bindings.
 */
#pragma once

#include <filesystem>

#include <libpkgexec/model.h>
#include <libpkgexec-linux/export.h>

namespace pkgexec_linux {

class PKGEXEC_LINUX_API interpreter_binding final {
public:
  [[nodiscard]] static interpreter_binding inspect(
      const std::filesystem::path& executable);
  [[nodiscard]] const pkgexec::interpreter_identity& identity() const noexcept;
  [[nodiscard]] const std::filesystem::path& executable() const noexcept;
  [[nodiscard]] const pkgexec::sha256_digest& content_digest() const noexcept;
  friend PKGEXEC_LINUX_API bool operator==(const interpreter_binding& lhs,
                         const interpreter_binding& rhs) noexcept;
  friend PKGEXEC_LINUX_API bool operator!=(const interpreter_binding& lhs,
                         const interpreter_binding& rhs) noexcept;
  friend PKGEXEC_LINUX_API bool operator<(const interpreter_binding& lhs,
                        const interpreter_binding& rhs) noexcept;
private:
  interpreter_binding(pkgexec::interpreter_identity identity,
                      std::filesystem::path executable,
                      pkgexec::sha256_digest content_digest);
  pkgexec::interpreter_identity identity_;
  std::filesystem::path executable_;
  pkgexec::sha256_digest content_digest_;
};

} // namespace pkgexec_linux
