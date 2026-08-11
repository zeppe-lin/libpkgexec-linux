// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#include <libpkgexec-linux/libpkgexec-linux.h>

#include <cstdlib>
#include <memory>

int main(int argc, char** argv)
{
  if (argc != 2)
    return 2;

  using namespace pkgexec_linux;
  const auto binding = interpreter_binding::inspect(argv[1]);
  std::unique_ptr<pkgexec::execution_backend> backend =
      std::make_unique<host_supervisor_backend>(
          host_supervisor_backend::make({binding}));
  const auto profile = backend->capabilities();
  if (profile.identity().hex().empty())
    return EXIT_FAILURE;

  try {
    (void)capability_observation(
        static_cast<capability_kind>(255), capability_state::available);
  } catch (const error& value) {
    return value.code() == error_code::invalid_value ? EXIT_SUCCESS : 3;
  }
  return 4;
}
