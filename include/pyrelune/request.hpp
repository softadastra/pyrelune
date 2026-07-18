/**
 * @file request.hpp
 * @brief Defines execution requests received from Vix Note.
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

#include <string>

namespace pyrelune
{
  /**
   * @brief Represents one execution request from Vix Note.
   */
  struct Request
  {
    /**
     * @brief Extension protocol identifier.
     */
    std::string protocol;

    /**
     * @brief Identifier used to correlate the response.
     */
    std::string request_id;

    /**
     * @brief Identifier of the Vix Note cell being executed.
     */
    std::string cell_id;

    /**
     * @brief Python source code to execute.
     */
    std::string source;

    /**
     * @brief Working directory requested by Vix Note.
     */
    std::string working_directory;
  };
}
