// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#include "../support/test.h"
#include "../support/temporary_directory.h"

#include <libpkgexec-linux/error.h>
#include <libpkgexec-linux/interpreter.h>

#include <filesystem>
#include <fstream>

namespace {

bool rejected(const std::filesystem::path& path,
              pkgexec_linux::error_code expected)
{
  try {
    (void)pkgexec_linux::interpreter_binding::inspect(path);
  } catch (const pkgexec_linux::error& value) {
    return value.code() == expected;
  }
  return false;
}

int test()
{
  using namespace pkgexec_linux;

  const auto first = interpreter_binding::inspect("/bin/sh");
  const auto second = interpreter_binding::inspect("/bin/sh");
  CHECK(first == second);
  CHECK(first.executable().is_absolute());
  CHECK(first.executable() == std::filesystem::canonical("/bin/sh"));
  CHECK(first.content_digest().hex().size() == 64U);
  CHECK(first.identity().hex().size() == 64U);

  CHECK(rejected("relative/sh", error_code::invalid_value));
  CHECK(rejected("/definitely/not/a/pkgexec/interpreter",
                 error_code::interpreter_inspection_failed));

  test_support::temporary_directory temporary;
  CHECK(rejected(temporary.path(), error_code::interpreter_inspection_failed));

  const auto copy = temporary.path() / "copy";
  std::filesystem::copy_file(first.executable(), copy);
  std::filesystem::permissions(
      copy, std::filesystem::perms::owner_read |
                std::filesystem::perms::owner_write);
  CHECK(rejected(copy, error_code::interpreter_inspection_failed));

  std::filesystem::permissions(
      copy, std::filesystem::perms::owner_read |
                std::filesystem::perms::owner_write |
                std::filesystem::perms::owner_exec);
  const auto copied = interpreter_binding::inspect(copy);
  CHECK(copied.identity() == first.identity());
  CHECK(copied.content_digest() == first.content_digest());
  CHECK(copied.executable() != first.executable());
  CHECK(copied != first);
  return 0;
}

} // namespace

int main() { return run_test(test); }
