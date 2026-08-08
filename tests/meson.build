# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
if get_option('tests').enabled()
  unit_cases = [
    ['capability-value', 'unit/capability_value_test.cpp'],
    ['backend-construction', 'unit/backend_construction_test.cpp'],
  ]
  foreach case : unit_cases
    target = executable(
      case[0] + '-test',
      case[1],
      dependencies: libpkgexec_linux_dep,
      install: false,
    )
    test(case[0], target, suite: 'unit')
  endforeach

  network_probe = executable(
    'network-probe',
    'fixtures/payloads/network_probe.cpp',
    override_options: ['b_sanitize=none'],
    install: false,
  )
  cancellation_probe = executable(
    'cancellation-probe',
    'fixtures/payloads/cancellation_probe.cpp',
    override_options: ['b_sanitize=none'],
    install: false,
  )
  resource_limit_probe = executable(
    'resource-limit-probe',
    'fixtures/payloads/resource_limit_probe.cpp',
    override_options: ['b_sanitize=none'],
    install: false,
  )

  foreach case : [
    ['capability-probe', 'integration/capability_probe_test.cpp'],
    ['interpreter-binding', 'integration/interpreter_binding_test.cpp'],
    ['host-execution', 'integration/host_execution_test.cpp'],
    ['host-admission', 'integration/host_admission_test.cpp'],
  ]
    target = executable(
      case[0] + '-test',
      case[1],
      dependencies: libpkgexec_linux_dep,
      install: false,
    )
    test(case[0], target, suite: 'integration')
  endforeach

  host_limit_test = executable(
    'host-resource-limit-test',
    'integration/host_resource_limit_test.cpp',
    dependencies: libpkgexec_linux_dep,
    install: false,
  )
  test(
    'host-resource-limit',
    host_limit_test,
    args: [resource_limit_probe.full_path()],
    suite: 'integration',
  )

  host_cancellation_test = executable(
    'host-cancellation-test',
    'integration/host_cancellation_test.cpp',
    dependencies: libpkgexec_linux_dep,
    install: false,
  )
  test(
    'host-cancellation',
    host_cancellation_test,
    args: [cancellation_probe.full_path()],
    suite: 'integration',
  )

  isolated_filesystem_test = executable(
    'isolated-filesystem-test',
    'privileged/isolated_filesystem_test.cpp',
    dependencies: libpkgexec_linux_dep,
    install: false,
  )
  test(
    'isolated-filesystem',
    isolated_filesystem_test,
    suite: 'integration-privileged',
  )

  isolated_network_test = executable(
    'isolated-network-test',
    'privileged/isolated_network_test.cpp',
    dependencies: libpkgexec_linux_dep,
    install: false,
  )
  test(
    'isolated-network',
    isolated_network_test,
    args: [network_probe.full_path()],
    suite: 'integration-privileged',
  )

  isolated_limit_test = executable(
    'isolated-resource-limit-test',
    'privileged/isolated_resource_limit_test.cpp',
    dependencies: libpkgexec_linux_dep,
    install: false,
  )
  test(
    'isolated-resource-limit',
    isolated_limit_test,
    args: [resource_limit_probe.full_path()],
    suite: 'integration-privileged',
  )

  isolated_cancellation_test = executable(
    'isolated-cancellation-test',
    'privileged/isolated_cancellation_test.cpp',
    dependencies: libpkgexec_linux_dep,
    install: false,
  )
  test(
    'isolated-cancellation',
    isolated_cancellation_test,
    args: [cancellation_probe.full_path()],
    suite: 'integration-privileged',
  )

  isolated_composition_test = executable(
    'isolated-composition-test',
    'privileged/isolated_composition_test.cpp',
    dependencies: libpkgexec_linux_dep,
    install: false,
  )
  test(
    'isolated-composition',
    isolated_composition_test,
    args: [cancellation_probe.full_path(), resource_limit_probe.full_path()],
    suite: 'integration-privileged',
  )

  foreach header : [
    'backend.h',
    'capability.h',
    'error.h',
    'interpreter.h',
  ]
    target = executable(
      'public-header-' + header.underscorify(),
      'header/public_header_test.cpp',
      cpp_args: ['-DTEST_HEADER=<libpkgexec-linux/' + header + '>'],
      dependencies: libpkgexec_linux_dep,
      install: false,
    )
    test('public-header-' + header, target, suite: 'header')
  endforeach
  umbrella = executable(
    'umbrella-header-test',
    'header/umbrella_header_test.cpp',
    dependencies: libpkgexec_linux_dep,
    install: false,
  )
  test('umbrella-header', umbrella, suite: 'header')

  sh = find_program('sh')
  test(
    'pkgconfig-metadata',
    sh,
    args: [
      join_paths(meson.current_source_dir(), 'contracts/check_pkgconfig_metadata.sh'),
      meson.project_build_root(),
      meson.project_version(),
    ],
    suite: 'contract',
  )
  test(
    'release-metadata',
    sh,
    args: [
      join_paths(meson.current_source_dir(), 'contracts/check_release_metadata.sh'),
      meson.project_source_root(),
      meson.project_version(),
    ],
    suite: 'contract',
  )
  test(
    'authority-contract',
    sh,
    args: [
      join_paths(meson.current_source_dir(), 'contracts/check_authority_contract.sh'),
      meson.project_source_root(),
    ],
    suite: 'contract',
  )
  test(
    'test-layout',
    sh,
    args: [
      join_paths(meson.current_source_dir(), 'contracts/check_test_layout.sh'),
      meson.project_source_root(),
    ],
    suite: 'contract',
  )
endif
