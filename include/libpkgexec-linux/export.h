// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#if defined(_WIN32) || defined(__CYGWIN__)
#if defined(PKGEXEC_LINUX_BUILDING_LIBRARY)
#define PKGEXEC_LINUX_API __declspec(dllexport)
#else
#define PKGEXEC_LINUX_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) || defined(__clang__)
#define PKGEXEC_LINUX_API __attribute__((visibility("default")))
#else
#define PKGEXEC_LINUX_API
#endif
