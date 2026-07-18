/**
 * @file kernel.cpp
 * @brief Implements the Pyrelune Python execution kernel.
 *
 * @author Softadastra
 *
 * @copyright
 * Copyright (c) 2026 Softadastra.
 *
 * @license
 * This project is licensed under the MIT License.
 */

#include <pyrelune/kernel.hpp>
#include <pyrelune/protocol.hpp>
#include <pyrelune/version.hpp>

#include <cctype>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>

namespace pyrelune
{
  namespace
  {
    [[nodiscard]] std::string escape_json(
        std::string_view value)
    {
      std::string output;
      output.reserve(value.size() + 16);

      constexpr char digits[] = "0123456789abcdef";

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

    [[nodiscard]] std::string serialize_runtime_request(
        const Request &request)
    {
      std::string json;

      json.reserve(
          request.source.size() +
          request.working_directory.size() +
          request.request_id.size() +
          request.cell_id.size() +
          128);

      json += "{\"protocol\":\"";
      json += escape_json(request.protocol);
      json += "\",\"requestId\":\"";
      json += escape_json(request.request_id);
      json += "\",\"cellId\":\"";
      json += escape_json(request.cell_id);
      json += "\",\"source\":\"";
      json += escape_json(request.source);
      json += "\",\"workingDirectory\":\"";
      json += escape_json(request.working_directory);
      json += "\"}";

      return json;
    }

    [[nodiscard]] bool contains_json_boolean(
        std::string_view json,
        std::string_view field,
        bool expected)
    {
      const std::string pattern =
          "\"" + std::string(field) + "\":" +
          (expected ? "true" : "false");

      return json.find(pattern) != std::string_view::npos;
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

    [[nodiscard]] std::string parse_json_string_value(
        std::string_view input,
        std::size_t position)
    {
      skip_whitespace(input, position);

      if (position >= input.size() || input[position] != '"')
      {
        return {};
      }

      ++position;

      std::string value;

      while (position < input.size())
      {
        const char character = input[position++];

        if (character == '"')
        {
          return value;
        }

        if (character != '\\')
        {
          value += character;
          continue;
        }

        if (position >= input.size())
        {
          return {};
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
          return {};
        }
      }

      return {};
    }

    [[nodiscard]] std::string find_json_string(
        std::string_view json,
        std::string_view field)
    {
      const std::string key =
          "\"" + std::string(field) + "\"";

      std::size_t position = json.find(key);

      if (position == std::string_view::npos)
      {
        return {};
      }

      position += key.size();
      skip_whitespace(json, position);

      if (position >= json.size() || json[position] != ':')
      {
        return {};
      }

      ++position;

      return parse_json_string_value(json, position);
    }

    [[nodiscard]] Response parse_runtime_response(
        const Request &request,
        const ProcessResult &process)
    {
      Response response;

      response.ok =
          contains_json_boolean(
              process.stdout_text,
              "ok",
              true);

      response.request_id =
          find_json_string(
              process.stdout_text,
              "requestId");

      response.stdout_text =
          find_json_string(
              process.stdout_text,
              "stdout");

      response.stderr_text =
          find_json_string(
              process.stdout_text,
              "stderr");

      response.error =
          find_json_string(
              process.stdout_text,
              "error");

      if (response.request_id.empty())
      {
        response.request_id = request.request_id;
      }

      if (
          process.exit_code != 0 &&
          response.error.empty())
      {
        response.ok = false;

        response.error =
            process.stderr_text.empty()
                ? "Python runtime exited with code " +
                      std::to_string(process.exit_code)
                : process.stderr_text;
      }

      if (
          !process.stderr_text.empty() &&
          response.stderr_text.empty())
      {
        response.stderr_text = process.stderr_text;
      }

      if (!response.ok && !response.error.empty())
      {
        response.diagnostics.push_back(
            Diagnostic{
                .severity = "error",
                .message = response.error,
                .code = "PythonExecutionError",
                .line = 0,
                .column = 0});
      }

      return response;
    }
  }

  Kernel::Kernel(KernelOptions options)
      : options_(std::move(options))
  {
  }

  Result<Response> Kernel::execute(
      const Request &request) const
  {
    if (request.protocol != note_extension_api)
    {
      return Result<Response>::failure(
          Error{
              ErrorCode::invalid_protocol,
              "Unsupported Vix Note extension protocol: " +
                  request.protocol});
    }

    if (request.request_id.empty())
    {
      return Result<Response>::failure(
          Error{
              ErrorCode::missing_field,
              "The requestId field cannot be empty"});
    }

    if (request.cell_id.empty())
    {
      return Result<Response>::failure(
          Error{
              ErrorCode::missing_field,
              "The cellId field cannot be empty"});
    }

    if (options_.runtime_script.empty())
    {
      return Result<Response>::failure(
          Error{
              ErrorCode::invalid_request,
              "The Pyrelune runtime script path is not configured"});
    }

    if (!std::filesystem::is_regular_file(
            options_.runtime_script))
    {
      return Result<Response>::failure(
          Error{
              ErrorCode::io_error,
              "The Pyrelune runtime script was not found: " +
                  options_.runtime_script.string()});
    }

    PythonProcessOptions process_options{
        .executable = options_.python_executable,
        .runtime_script = options_.runtime_script,
        .working_directory = request.working_directory,
        .timeout = options_.timeout,
        .arguments = {}};

    const std::string input =
        serialize_runtime_request(request);

    auto process_result =
        run_python_process(process_options, input);

    if (!process_result)
    {
      return Result<Response>::failure(
          process_result.error());
    }

    Response response =
        parse_runtime_response(
            request,
            process_result.value());

    return Result<Response>::success(
        std::move(response));
  }

  const KernelOptions &Kernel::options() const noexcept
  {
    return options_;
  }
}
