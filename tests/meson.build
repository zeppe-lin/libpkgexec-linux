# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
if get_option('tests').enabled()
  foreach test_name : ['capability','interpreter']
    test_binary = executable(test_name + '-test',test_name + '_test.cpp',include_directories: include_directories('.'),dependencies: libpkgexec_linux_dep,install: false)
    test(test_name,test_binary)
  endforeach
  foreach header : ['libpkgexec-linux/libpkgexec-linux.h','libpkgexec-linux/capability.h','libpkgexec-linux/error.h','libpkgexec-linux/interpreter.h']
    name = 'public-' + header.split('/')[1].split('.')[0]
    public_header = executable(name,'public_headers.cpp',cpp_args: ['-DTEST_HEADER=<' + header + '>'],dependencies: libpkgexec_linux_dep,install: false)
    test(name,public_header)
  endforeach
endif
