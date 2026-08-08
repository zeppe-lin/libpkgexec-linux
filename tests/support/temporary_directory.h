// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <filesystem>
#include <stdexcept>
#include <string>

#include <cstdlib>

namespace test_support {

class temporary_directory final {
public:
  explicit temporary_directory(std::string pattern = "/tmp/pkgexec-linux-test.XXXXXX")
  {
    char* value = ::mkdtemp(pattern.data());
    if (!value) {
      throw std::runtime_error("mkdtemp failed");
    }
    path_ = value;
  }

  ~temporary_directory()
  {
    std::error_code ignored;
    std::filesystem::permissions(
        path_, std::filesystem::perms::owner_all,
        std::filesystem::perm_options::add, ignored);
    std::filesystem::remove_all(path_, ignored);
  }

  temporary_directory(const temporary_directory&) = delete;
  temporary_directory& operator=(const temporary_directory&) = delete;

  [[nodiscard]] const std::filesystem::path& path() const noexcept
  {
    return path_;
  }

private:
  std::filesystem::path path_;
};

} // namespace test_support
