/**
 * @file error.hpp
 * @brief Defines errors produced by the Pyrelune runtime.
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
#include <string_view>
#include <utility>

namespace pyrelune
{
  /**
   * @brief Identifies a category of Pyrelune error.
   */
  enum class ErrorCode
  {
    invalid_request,
    invalid_protocol,
    invalid_json,
    missing_field,
    python_not_found,
    process_failed,
    execution_failed,
    timeout,
    io_error,
    internal_error
  };

  /**
   * @brief Returns a stable textual representation of an error code.
   *
   * @param code Error code to convert.
   * @return Stable error code name.
   */
  [[nodiscard]] constexpr std::string_view to_string(
      ErrorCode code) noexcept
  {
    switch (code)
    {
    case ErrorCode::invalid_request:
      return "invalid_request";
    case ErrorCode::invalid_protocol:
      return "invalid_protocol";
    case ErrorCode::invalid_json:
      return "invalid_json";
    case ErrorCode::missing_field:
      return "missing_field";
    case ErrorCode::python_not_found:
      return "python_not_found";
    case ErrorCode::process_failed:
      return "process_failed";
    case ErrorCode::execution_failed:
      return "execution_failed";
    case ErrorCode::timeout:
      return "timeout";
    case ErrorCode::io_error:
      return "io_error";
    case ErrorCode::internal_error:
      return "internal_error";
    }

    return "internal_error";
  }

  /**
   * @brief Represents a structured Pyrelune error.
   */
  class Error
  {
  public:
    /**
     * @brief Constructs an error.
     *
     * @param code Error category.
     * @param message Human-readable error message.
     */
    Error(ErrorCode code, std::string message)
        : code_(code),
          message_(std::move(message))
    {
    }

    /**
     * @brief Returns the error category.
     */
    [[nodiscard]] ErrorCode code() const noexcept
    {
      return code_;
    }

    /**
     * @brief Returns the human-readable error message.
     */
    [[nodiscard]] const std::string &message() const noexcept
    {
      return message_;
    }

  private:
    ErrorCode code_;
    std::string message_;
  };
}
