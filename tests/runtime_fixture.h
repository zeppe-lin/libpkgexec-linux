// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <elf.h>

namespace runtime_fixture {

inline bool has_program_interpreter(const std::filesystem::path& executable)
{
  std::ifstream stream(executable, std::ios::binary);
  Elf64_Ehdr header{};
  stream.read(reinterpret_cast<char*>(&header), sizeof(header));
  if (!stream || std::memcmp(header.e_ident, ELFMAG, SELFMAG) != 0 ||
      header.e_ident[EI_CLASS] != ELFCLASS64 ||
      header.e_ident[EI_DATA] != ELFDATA2LSB ||
      header.e_machine != EM_X86_64 ||
      header.e_phentsize != sizeof(Elf64_Phdr) ||
      header.e_phnum == PN_XNUM) {
    throw std::runtime_error(
        "runtime fixture is not a supported x86-64 ELF executable");
  }
  stream.seekg(static_cast<std::streamoff>(header.e_phoff));
  if (!stream) {
    throw std::runtime_error("cannot inspect runtime fixture program headers");
  }
  for (Elf64_Half index = 0; index < header.e_phnum; ++index) {
    Elf64_Phdr program_header{};
    stream.read(reinterpret_cast<char*>(&program_header),
                sizeof(program_header));
    if (!stream) {
      throw std::runtime_error(
          "cannot inspect runtime fixture program headers");
    }
    if (program_header.p_type == PT_INTERP) {
      return true;
    }
  }
  return false;
}

inline void copy_one(const std::filesystem::path& root,
                     const std::filesystem::path& source,
                     const std::filesystem::path& logical_path)
{
  const auto destination = root / logical_path.relative_path();
  std::filesystem::create_directories(destination.parent_path());
  std::filesystem::copy_file(
      source, destination, std::filesystem::copy_options::overwrite_existing);
  std::filesystem::permissions(
      destination, std::filesystem::status(source).permissions());
}

inline void copy_runtime(
    const std::filesystem::path& root,
    const std::filesystem::path& executable,
    const std::filesystem::path& logical_executable = {})
{
  copy_one(root, executable,
           logical_executable.empty() ? executable : logical_executable);
  if (!has_program_interpreter(executable)) {
    return;
  }
  const std::string command = "ldd '" + executable.string() + "'";
  FILE* stream = ::popen(command.c_str(), "r");
  if (!stream) {
    throw std::runtime_error("cannot inspect runtime fixture closure");
  }
  std::array<char, 4096> line{};
  std::vector<std::filesystem::path> dependencies;
  while (::fgets(line.data(), static_cast<int>(line.size()), stream)) {
    std::string value(line.data());
    std::size_t start = value.find("=> /");
    if (start != std::string::npos) {
      start += 3U;
    } else {
      start = value.find('/');
    }
    if (start == std::string::npos) {
      continue;
    }
    const auto end = value.find_first_of(" \t\n", start);
    const auto path = value.substr(start, end - start);
    if (!path.empty() && path.front() == '/') {
      dependencies.emplace_back(path);
    }
  }
  const int status = ::pclose(stream);
  if (status != 0) {
    throw std::runtime_error("ldd failed for the runtime fixture");
  }
  std::sort(dependencies.begin(), dependencies.end());
  dependencies.erase(std::unique(dependencies.begin(), dependencies.end()),
                     dependencies.end());
  for (const auto& dependency : dependencies) {
    copy_one(root, dependency, dependency);
  }
}

} // namespace runtime_fixture
