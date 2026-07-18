/**
 * @file protocol.cpp
 * @brief Implements the Vix Note extension protocol.
 *
 * @author Softadastra
 *
 * @copyright
 * Copyright (c) 2026 Softadastra.
 *
 * @license
 * This project is licensed under the MIT License.
 */

#include <pyrelune/protocol.hpp>
#include <pyrelune/version.hpp>

#include <charconv>
#include <cctype>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

namespace pyrelune
{
  namespace
  {
    [[nodiscard]] std::string escape_json(std::string_view value)
    {
      std::string output;
      output.reserve(value.size() + 16);

      for (const unsigned char character : value)
      {
        switch (character)
        {
        case '"':
          output += "\\\"";
          break;
        case '\\':
          output += "\\\\";
          break;
        case '\b':
          output += "\\b";
          break;
        case '\f':
          output += "\\f";
          break;
        case '\n':
          output += "\\n";
          break;
        case '\r':
          output += "\\r";
          break;
        case '\t':
          output += "\\t";
          break;
        default:
          if (character < 0x20)
          {
            constexpr char digits[] = "0123456789abcdef";

            output += "\\u00";
            output += digits[(character >> 4) & 0x0f];
            output += digits[character & 0x0f];
          }
          else
          {
            output += static_cast<char>(character);
          }

          break;
        }
      }

      return output;
    }

    void skip_whitespace(
        std::string_view input,
        std::size_t &position) noexcept
    {
      while (
          position < input.size() &&
          std::isspace(
              static_cast<unsigned char>(input[position])) != 0)
      {
        ++position;
      }
    }

    [[nodiscard]] Result<std::string> parse_json_string(
        std::string_view input,
        std::size_t &position)
    {
      skip_whitespace(input, position);

      if (position >= input.size() || input[position] != '"')
      {
        return Result<std::string>::failure(
            Error{
                ErrorCode::invalid_json,
                "Expected a JSON string"});
      }

      ++position;

      std::string value;

      while (position < input.size())
      {
        const char character = input[position++];

        if (character == '"')
        {
          return Result<std::string>::success(
              std::move(value));
        }

        if (character != '\\')
        {
          value += character;
          continue;
        }

        if (position >= input.size())
        {
          return Result<std::string>::failure(
              Error{
                  ErrorCode::invalid_json,
                  "Incomplete JSON escape sequence"});
        }

        const char escaped = input[position++];

        switch (escaped)
        {
        case '"':
          value += '"';
          break;
        case '\\':
          value += '\\';
          break;
        case '/':
          value += '/';
          break;
        case 'b':
          value += '\b';
          break;
        case 'f':
          value += '\f';
          break;
        case 'n':
          value += '\n';
          break;
        case 'r':
          value += '\r';
          break;
        case 't':
          value += '\t';
          break;
        default:
          return Result<std::string>::failure(
              Error{
                  ErrorCode::invalid_json,
                  "Unsupported JSON escape sequence"});
        }
      }

      return Result<std::string>::failure(
          Error{
              ErrorCode::invalid_json,
              "Unterminated JSON string"});
    }

    [[nodiscard]] Result<std::string> find_string_field(
        std::string_view input,
        std::string_view field,
        bool required)
    {
      const std::string quoted_field =
          "\"" + std::string(field) + "\"";

      std::size_t position = input.find(quoted_field);

      if (position == std::string_view::npos)
      {
        if (!required)
        {
          return Result<std::string>::success({});
        }

        return Result<std::string>::failure(
            Error{
                ErrorCode::missing_field,
                "Missing required field: " +
                    std::string(field)});
      }

      position += quoted_field.size();
      skip_whitespace(input, position);

      if (position >= input.size() || input[position] != ':')
      {
        return Result<std::string>::failure(
            Error{
                ErrorCode::invalid_json,
                "Expected ':' after field: " +
                    std::string(field)});
      }

      ++position;

      return parse_json_string(input, position);
    }

    void append_json_string_field(
        std::ostringstream &stream,
        std::string_view name,
        std::string_view value,
        bool &first)
    {
      if (!first)
      {
        stream << ',';
      }

      first = false;

      stream
          << '"' << name << "\":\""
          << escape_json(value)
          << '"';
    }

    void append_json_boolean_field(
        std::ostringstream &stream,
        std::string_view name,
        bool value,
        bool &first)
    {
      if (!first)
      {
        stream << ',';
      }

      first = false;

      stream
          << '"' << name << "\":"
          << (value ? "true" : "false");
    }
  }

  Result<Request> parse_request(std::string_view input)
  {
    std::size_t position = 0;
    skip_whitespace(input, position);

    if (position >= input.size() || input[position] != '{')
    {
      return Result<Request>::failure(
          Error{
              ErrorCode::invalid_json,
              "The request must be a JSON object"});
    }

    auto protocol = find_string_field(
        input,
        "protocol",
        true);

    if (!protocol)
    {
      return Result<Request>::failure(protocol.error());
    }

    if (protocol.value() != note_extension_api)
    {
      return Result<Request>::failure(
          Error{
              ErrorCode::invalid_protocol,
              "Unsupported protocol: " +
                  protocol.value()});
    }

    auto request_id = find_string_field(
        input,
        "requestId",
        true);

    if (!request_id)
    {
      return Result<Request>::failure(request_id.error());
    }

    auto cell_id = find_string_field(
        input,
        "cellId",
        true);

    if (!cell_id)
    {
      return Result<Request>::failure(cell_id.error());
    }

    auto source = find_string_field(
        input,
        "source",
        true);

    if (!source)
    {
      return Result<Request>::failure(source.error());
    }

    auto working_directory = find_string_field(
        input,
        "workingDirectory",
        false);

    if (!working_directory)
    {
      return Result<Request>::failure(
          working_directory.error());
    }

    Request request{
        .protocol = protocol.take_value(),
        .request_id = request_id.take_value(),
        .cell_id = cell_id.take_value(),
        .source = source.take_value(),
        .working_directory =
            working_directory.take_value()};

    return Result<Request>::success(std::move(request));
  }

  std::string serialize_response(const Response &response)
  {
    std::ostringstream stream;
    stream << '{';

    bool first = true;

    append_json_boolean_field(
        stream,
        "ok",
        response.ok,
        first);

    append_json_string_field(
        stream,
        "requestId",
        response.request_id,
        first);

    append_json_string_field(
        stream,
        "stdout",
        response.stdout_text,
        first);

    append_json_string_field(
        stream,
        "stderr",
        response.stderr_text,
        first);

    append_json_string_field(
        stream,
        "error",
        response.error,
        first);

    stream << ",\"outputs\":[";

    for (std::size_t index = 0;
         index < response.outputs.size();
         ++index)
    {
      if (index != 0)
      {
        stream << ',';
      }

      const auto &output = response.outputs[index];

      stream
          << "{\"mime\":\""
          << escape_json(output.mime)
          << "\",\"data\":\""
          << escape_json(output.data)
          << "\"}";
    }

    stream << ']';
    stream << ",\"diagnostics\":[";

    for (std::size_t index = 0;
         index < response.diagnostics.size();
         ++index)
    {
      if (index != 0)
      {
        stream << ',';
      }

      const auto &diagnostic =
          response.diagnostics[index];

      stream
          << "{\"severity\":\""
          << escape_json(diagnostic.severity)
          << "\",\"message\":\""
          << escape_json(diagnostic.message)
          << "\",\"code\":\""
          << escape_json(diagnostic.code)
          << "\",\"line\":"
          << diagnostic.line
          << ",\"column\":"
          << diagnostic.column
          << '}';
    }

    stream << "]}";

    return stream.str();
  }
}
