// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#include <libpkgexec-linux/libpkgexec-linux.h>
#include <libpkgexec/libpkgexec.h>

#include <cstddef>

#if !defined(__x86_64__)
#error "libpkgexec-linux 0.6 ABI layout qualification is x86-64 specific"
#endif

static_assert(sizeof(void*) == 8);
static_assert(alignof(void*) == 8);
static_assert(sizeof(pkgexec::execution_backend) == 8);
static_assert(sizeof(pkgexec::controlled_execution_backend) == 8);
static_assert(sizeof(pkgexec::backend_capability_profile) == 88);
static_assert(sizeof(pkgexec::interpreter_identity) == 32);
static_assert(sizeof(pkgexec::execution_request) == 720);
static_assert(sizeof(pkgexec::execution_result) == 1160);
static_assert(sizeof(pkgexec_linux::capability_observation) == 40);
static_assert(sizeof(pkgexec_linux::capability_report) == 112);
static_assert(sizeof(pkgexec_linux::interpreter_binding) == 104);
static_assert(sizeof(pkgexec_linux::host_supervisor_backend) == 144);
static_assert(sizeof(pkgexec_linux::isolated_backend) == 144);

int main() { return 0; }
