# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
project(
  'libpkgexec-linux',
  'cpp',
  version: '0.5.1',
  license: 'GPL-3.0-or-later',
  meson_version: '>=1.2.0',
  default_options: [
    'cpp_std=c++17',
    'warning_level=3',
    'default_library=shared',
    'b_ndebug=if-release',
  ],
)

if host_machine.system() != 'linux'
  error('libpkgexec-linux requires Linux')
endif
if host_machine.cpu_family() != 'x86_64'
  error('libpkgexec-linux supports x86_64 only')
endif

library_kind = get_option('default_library')
if library_kind == 'both'
  error('libpkgexec-linux requires separate shared and static builds')
endif
want_static = get_option('link_mode') == 'static'
if (library_kind == 'static') != want_static
  error('default_library and link_mode must both select shared or static')
endif

threads_dep = dependency('threads')
libcrypto_dep = dependency('libcrypto', required: true, static: want_static)
libpkgexec_dep = dependency(
  'libpkgexec',
  version: '>=1.2.0',
  required: true,
  static: want_static,
  fallback: ['libpkgexec', 'libpkgexec_dep'],
  default_options: [
    'default_library=' + library_kind,
    'link_mode=' + get_option('link_mode'),
    'tests=disabled',
    'man_pages=disabled',
  ],
)
pkgconf = import('pkgconfig')

subdir('src')
subdir('man')
subdir('tests')
