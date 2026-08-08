// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

#define CHECK(expression)                                                      \
  do {                                                                         \
    if (!(expression)) {                                                       \
      throw std::runtime_error(std::string("check failed: ") + #expression + \
                               " at " + __FILE__ + ":" +                    \
                               std::to_string(__LINE__));                       \
    }                                                                          \
  } while (false)

inline int run_test(int (*function)())
{
  try {
    return function();
  } catch (const std::exception& value) {
    std::cerr << value.what() << '\n';
    return EXIT_FAILURE;
  }
}
