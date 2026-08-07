# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
public_inc = include_directories('../include')
private_inc = include_directories('.')

libpkgexec_linux = library(
  'pkgexec-linux',
  [
    'backend.cpp',
    'capability.cpp',
    'error.cpp',
    'interpreter.cpp',
    'mount_isolation.cpp',
    'network_isolation.cpp',
    'process_control.cpp',
    'resource_limits.cpp',
    'support.cpp',
  ],
  include_directories: [public_inc, private_inc],
  dependencies: [libpkgexec_dep, libcrypto_dep, threads_dep],
  install: true,
  version: meson.project_version(),
  soversion: '1',
)

libpkgexec_linux_dep = declare_dependency(
  link_with: libpkgexec_linux,
  include_directories: public_inc,
  dependencies: [libpkgexec_dep, libcrypto_dep, threads_dep],
)
meson.override_dependency('libpkgexec-linux', libpkgexec_linux_dep)

install_headers(
  '../include/libpkgexec-linux/libpkgexec-linux.h',
  '../include/libpkgexec-linux/backend.h',
  '../include/libpkgexec-linux/capability.h',
  '../include/libpkgexec-linux/error.h',
  '../include/libpkgexec-linux/interpreter.h',
  subdir: 'libpkgexec-linux',
)

pkgconf.generate(
  libpkgexec_linux,
  name: 'libpkgexec-linux',
  filebase: 'libpkgexec-linux',
  description: 'Linux execution backends for libpkgexec',
  subdirs: '.',
  requires_private: ['libcrypto'],
  libraries_private: [threads_dep],
)
