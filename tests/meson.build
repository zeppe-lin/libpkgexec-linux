# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
if get_option('tests').enabled()
  foreach test_name : [
    'capability',
    'interpreter',
    'backend',
  ]
    test_binary = executable(
      test_name + '-test',
      test_name + '_test.cpp',
      include_directories: include_directories('.'),
      dependencies: libpkgexec_linux_dep,
      install: false,
    )
    test(test_name, test_binary)
  endforeach

  network_probe = executable(
    'network-probe',
    'network_probe.cpp',
    install: false,
  )
  cancellation_probe = executable(
    'cancellation-probe',
    'cancellation_probe.cpp',
    install: false,
  )
  cancellation_test = executable(
    'cancellation-test',
    'cancellation_test.cpp',
    include_directories: include_directories('.'),
    dependencies: libpkgexec_linux_dep,
    install: false,
  )
  test('cancellation', cancellation_test,
       args: [cancellation_probe.full_path()])

  resource_limit_probe = executable(
    'resource-limit-probe',
    'resource_limit_probe.cpp',
    install: false,
  )
  resource_limit_test = executable(
    'resource-limit-test',
    'resource_limit_test.cpp',
    include_directories: include_directories('.'),
    dependencies: libpkgexec_linux_dep,
    install: false,
  )
  test('resource-limit', resource_limit_test,
       args: [resource_limit_probe.full_path()])

  isolated_test = executable(
    'isolated-test',
    'isolated_test.cpp',
    include_directories: include_directories('.'),
    dependencies: libpkgexec_linux_dep,
    install: false,
  )
  test('isolated', isolated_test,
       args: [network_probe.full_path(), cancellation_probe.full_path(),
              resource_limit_probe.full_path()])

  foreach header : [
    'libpkgexec-linux/libpkgexec-linux.h',
    'libpkgexec-linux/backend.h',
    'libpkgexec-linux/capability.h',
    'libpkgexec-linux/error.h',
    'libpkgexec-linux/interpreter.h',
  ]
    name = 'public-' + header.split('/')[1].split('.')[0]
    public_header = executable(
      name,
      'public_headers.cpp',
      cpp_args: ['-DTEST_HEADER=<' + header + '>'],
      dependencies: libpkgexec_linux_dep,
      install: false,
    )
    test(name, public_header)
  endforeach

  sh = find_program('sh')
  test(
    'metadata',
    sh,
    args: [
      join_paths(meson.current_source_dir(), 'metadata_test.sh'),
      meson.project_build_root(),
    ],
  )
  test(
    'release-metadata',
    sh,
    args: [
      join_paths(meson.current_source_dir(), 'check_release_metadata.sh'),
      meson.project_source_root(),
    ],
  )
  test(
    'authority-contract',
    sh,
    args: [
      join_paths(meson.current_source_dir(), 'check_authority_contract.sh'),
      meson.project_source_root(),
    ],
  )
endif
