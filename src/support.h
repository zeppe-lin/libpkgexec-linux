// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include <libpkgexec/model.h>

namespace pkgexec_linux::detail {

[[nodiscard]] std::string sha256_hex(std::string_view domain,
                                     std::string_view material);
[[nodiscard]] pkgexec::sha256_digest digest_file(
    const std::filesystem::path& path);
[[nodiscard]] pkgexec::sha256_digest digest_fd(int fd);
[[nodiscard]] pkgexec::interpreter_identity interpreter_identity(
    const pkgexec::sha256_digest& digest);
[[nodiscard]] bool path_has_symlink_component(
    const std::filesystem::path& path);
[[nodiscard]] std::vector<std::uint64_t> current_groups();
[[nodiscard]] std::string errno_message(std::string_view operation, int value);

} // namespace pkgexec_linux::detail
