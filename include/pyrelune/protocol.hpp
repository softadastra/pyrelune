/**
 * @file protocol.hpp
 * @brief Parses and serializes the Vix Note extension protocol.
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

#include <pyrelune/request.hpp>
#include <pyrelune/response.hpp>
#include <pyrelune/result.hpp>

#include <string>
#include <string_view>

namespace pyrelune
{
  /**
   * @brief Parses a JSON execution request received from Vix Note.
   *
   * @param input Complete JSON request.
   * @return Parsed request or a structured protocol error.
   */
  [[nodiscard]] Result<Request> parse_request(std::string_view input);

  /**
   * @brief Serializes an execution response as JSON.
   *
   * @param response Response to serialize.
   * @return JSON document compatible with Vix Note.
   */
  [[nodiscard]] std::string serialize_response(
      const Response &response);
}
