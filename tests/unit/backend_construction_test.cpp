// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#include "../support/test.h"

#include <libpkgexec-linux/libpkgexec-linux.h>

#include <vector>

namespace {

template <typename Factory>
bool rejects(Factory&& factory, pkgexec_linux::error_code expected)
{
  try {
    factory();
  } catch (const pkgexec_linux::error& value) {
    return value.code() == expected;
  }
  return false;
}

int test()
{
  using namespace pkgexec_linux;

  CHECK(rejects(
      [] { (void)host_supervisor_backend::make({}); },
      error_code::invalid_value));
  CHECK(rejects(
      [] { (void)isolated_backend::make({}); },
      error_code::invalid_value));

  const auto shell = interpreter_binding::inspect("/bin/sh");
  CHECK(rejects(
      [&] { (void)host_supervisor_backend::make({shell, shell}); },
      error_code::duplicate_interpreter));
  CHECK(rejects(
      [&] { (void)isolated_backend::make({shell, shell}); },
      error_code::duplicate_interpreter));
  return 0;
}

} // namespace

int main() { return run_test(test); }
