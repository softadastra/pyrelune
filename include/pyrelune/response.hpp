/**
 * @file response.hpp
 * @brief Defines execution responses returned to Vix Note.
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
#include <vector>

namespace pyrelune
{
  /**
   * @brief Represents a rich MIME output produced by Python code.
   */
  struct Output
  {
    /**
     * @brief MIME type of the output.
     */
    std::string mime;

    /**
     * @brief Serialized output content.
     */
    std::string data;
  };

  /**
   * @brief Represents one structured diagnostic.
   */
  struct Diagnostic
  {
    /**
     * @brief Diagnostic severity such as error, warning, or information.
     */
    std::string severity;

    /**
     * @brief Human-readable diagnostic message.
     */
    std::string message;

    /**
     * @brief Optional stable diagnostic code.
     */
    std::string code;

    /**
     * @brief One-based source line.
     */
    std::size_t line{0};

    /**
     * @brief One-based source column.
     */
    std::size_t column{0};
  };

  /**
   * @brief Represents the result of executing a Python cell.
   */
  struct Response
  {
    /**
     * @brief Indicates whether execution completed successfully.
     */
    bool ok{false};

    /**
     * @brief Identifier copied from the request.
     */
    std::string request_id;

    /**
     * @brief Text written to standard output.
     */
    std::string stdout_text;

    /**
     * @brief Text written to standard error.
     */
    std::string stderr_text;

    /**
     * @brief Human-readable execution error.
     */
    std::string error;

    /**
     * @brief Rich outputs produced during execution.
     */
    std::vector<Output> outputs;

    /**
     * @brief Structured diagnostics produced during execution.
     */
    std::vector<Diagnostic> diagnostics;
  };
}
