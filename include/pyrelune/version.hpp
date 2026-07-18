/**
 * @file version.hpp
 * @brief Provides compile-time version information for Pyrelune.
 *
 * @author Softadastra
 *
 * @copyright
 * Copyright (c) 2026 Softadastra.
 *
 * @license
 * This project is licensed under the MIT License.
 */

#pragma once

#include <string_view>

namespace pyrelune
{
  /**
   * @brief Major component of the current Pyrelune version.
   */
  inline constexpr int version_major = 0;

  /**
   * @brief Minor component of the current Pyrelune version.
   */
  inline constexpr int version_minor = 1;

  /**
   * @brief Patch component of the current Pyrelune version.
   */
  inline constexpr int version_patch = 0;

  /**
   * @brief Complete Pyrelune version string.
   */
  inline constexpr std::string_view version = "0.1.0";

  /**
   * @brief Vix Note extension protocol implemented by Pyrelune.
   */
  inline constexpr std::string_view note_extension_api =
      "vix-note-extension-1";
}
