// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <libpkgexec-linux/export.h>

#include <stdexcept>
#include <string>

namespace pkgexec_linux {

enum class error_code {
  invalid_value,
  duplicate_interpreter,
  interpreter_inspection_failed,
  unsupported_platform,
};

class PKGEXEC_LINUX_API error : public std::runtime_error {
public:
  error(error_code code, std::string message);
  ~error() override;
  [[nodiscard]] error_code code() const noexcept;
private:
  error_code code_;
};

} // namespace pkgexec_linux
