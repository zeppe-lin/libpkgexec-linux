# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
scdoc = find_program('scdoc', required: get_option('man_pages'))
if scdoc.found()
  pages = [
    ['libpkgexec-linux.3.scd', 'libpkgexec-linux.3', 'man3'],
    ['pkgexec_linux_backend.3.scd', 'pkgexec_linux_backend.3', 'man3'],
    ['pkgexec_linux_isolated.3.scd', 'pkgexec_linux_isolated.3', 'man3'],
    ['pkgexec_linux_capability.3.scd', 'pkgexec_linux_capability.3', 'man3'],
  ]
  foreach page : pages
    custom_target(
      page[1],
      input: page[0],
      output: page[1],
      command: [scdoc],
      feed: true,
      capture: true,
      install: true,
      install_dir: join_paths(get_option('mandir'), page[2]),
    )
  endforeach
endif
