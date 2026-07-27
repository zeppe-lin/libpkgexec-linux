// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#include "test.h"

#include <libpkgexec-linux/interpreter.h>
#include <libpkgexec-linux/error.h>

#include <filesystem>

namespace {

int test()
{
  const auto first = pkgexec_linux::interpreter_binding::inspect("/bin/sh");
  const auto second = pkgexec_linux::interpreter_binding::inspect("/bin/sh");
  CHECK(first == second);
  CHECK(first.executable().is_absolute());
  CHECK(first.content_digest().hex().size() == 64U);
  CHECK(first.identity().hex().size() == 64U);

  bool rejected = false;
  try {
    (void)pkgexec_linux::interpreter_binding::inspect("relative/sh");
  } catch (const pkgexec_linux::error& value) {
    rejected = value.code() == pkgexec_linux::error_code::invalid_value;
  }
  CHECK(rejected);
  return 0;
}

} // namespace

int main() { return run_test(test); }
