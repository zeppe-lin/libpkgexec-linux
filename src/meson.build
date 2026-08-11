# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
public_inc = include_directories('../include')
private_inc = include_directories('.')

elf_export_generator = find_program(
  meson.project_source_root() / 'tools/generate-elf-export-script.sh',
)
linux_export_script_name = 'libpkgexec-linux.exports.map'
linux_export_script_path = meson.current_build_dir() / linux_export_script_name
linux_export_script = configure_file(
  input: '../abi/libpkgexec-linux.exports',
  output: linux_export_script_name,
  command: [elf_export_generator, '@INPUT@'],
  capture: true,
)

visibility_cpp_args = []
if meson.get_compiler('cpp').has_argument('-fvisibility-inlines-hidden')
  visibility_cpp_args += ['-fvisibility-inlines-hidden']
endif

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
  dependencies: [libpkgexec_dep, libpkgsource_dep, libcrypto_dep, threads_dep],
  cpp_args: visibility_cpp_args + ['-DPKGEXEC_LINUX_BUILDING_LIBRARY'],
  gnu_symbol_visibility: 'hidden',
  link_args: ['-Wl,--version-script=' + linux_export_script_path],
  link_depends: [linux_export_script],
  install: true,
  version: meson.project_version(),
  soversion: '2',
)

libpkgexec_linux_dep = declare_dependency(
  link_with: libpkgexec_linux,
  include_directories: public_inc,
  dependencies: [libpkgexec_dep, libpkgsource_dep, libcrypto_dep, threads_dep],
)
meson.override_dependency('libpkgexec-linux', libpkgexec_linux_dep)

install_headers(
  '../include/libpkgexec-linux/libpkgexec-linux.h',
  '../include/libpkgexec-linux/backend.h',
  '../include/libpkgexec-linux/capability.h',
  '../include/libpkgexec-linux/error.h',
  '../include/libpkgexec-linux/export.h',
  '../include/libpkgexec-linux/interpreter.h',
  subdir: 'libpkgexec-linux',
)

pkgconf.generate(
  libpkgexec_linux,
  name: 'libpkgexec-linux',
  filebase: 'libpkgexec-linux',
  description: 'Linux execution backends for libpkgexec',
  subdirs: '.',
  requires: [libpkgexec_dep, libpkgsource_dep],
  requires_private: [libcrypto_dep],
  libraries_private: ['-pthread'],
)
