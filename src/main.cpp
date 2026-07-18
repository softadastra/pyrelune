/**
 * @file main.cpp
 * @brief Provides the Pyrelune command-line runtime entry point.
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

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
  [[nodiscard]] std::filesystem::path runtime_script_from_executable(
      const std::filesystem::path &executable)
  {
    std::error_code error;

    const auto canonical_executable =
        std::filesystem::weakly_canonical(
            executable,
            error);

    const auto executable_path =
        error ? executable : canonical_executable;

    const auto executable_directory =
        executable_path.parent_path();

    const std::vector<std::filesystem::path> candidates{
        executable_directory /
            "../share/pyrelune/pyrelune-runtime.py",
        executable_directory /
            "../share/pyrelune/scripts/pyrelune-runtime.py",
        executable_directory /
            "../scripts/pyrelune-runtime.py",
        executable_directory /
            "scripts/pyrelune-runtime.py",
        std::filesystem::current_path() /
            "scripts/pyrelune-runtime.py"};

    for (const auto &candidate : candidates)
    {
      const auto normalized =
          std::filesystem::weakly_canonical(
              candidate,
              error);

      if (
          !error &&
          std::filesystem::is_regular_file(normalized))
      {
        return normalized;
      }

      error.clear();

      if (std::filesystem::is_regular_file(candidate))
      {
        return candidate;
      }
    }

    return {};
  }

  [[nodiscard]] std::filesystem::path resolve_runtime_script(
      const std::filesystem::path &executable)
  {
    if (const char *configured =
            std::getenv("PYRELUNE_RUNTIME_SCRIPT"))
    {
      if (*configured != '\0')
      {
        return configured;
      }
    }

    return runtime_script_from_executable(executable);
  }

  [[nodiscard]] std::filesystem::path resolve_python_executable()
  {
    if (const char *configured =
            std::getenv("PYRELUNE_PYTHON"))
    {
      if (*configured != '\0')
      {
        return configured;
      }
    }

    return "python3";
  }

  [[nodiscard]] std::string read_standard_input()
  {
    return std::string{
        std::istreambuf_iterator<char>{std::cin},
        std::istreambuf_iterator<char>{}};
  }

  [[nodiscard]] pyrelune::Response error_response(
      const std::string &message,
      const std::string &request_id = {})
  {
    pyrelune::Response response;

    response.ok = false;
    response.request_id = request_id;
    response.error = message;

    response.diagnostics.push_back(
        pyrelune::Diagnostic{
            .severity = "error",
            .message = message,
            .code = "PyreluneRuntimeError",
            .line = 0,
            .column = 0});

    return response;
  }
}

int main(int argc, char **argv)
{
  const std::filesystem::path executable =
      argc > 0 && argv[0] != nullptr
          ? std::filesystem::path{argv[0]}
          : std::filesystem::path{"pyrelune"};

  const std::string input = read_standard_input();

  auto request_result =
      pyrelune::parse_request(input);

  if (!request_result)
  {
    const auto response =
        error_response(
            request_result.error().message());

    std::cout
        << pyrelune::serialize_response(response)
        << '\n';

    return 1;
  }

  const auto runtime_script =
      resolve_runtime_script(executable);

  if (runtime_script.empty())
  {
    const auto response =
        error_response(
            "Pyrelune runtime script was not found. "
            "Set PYRELUNE_RUNTIME_SCRIPT or reinstall Pyrelune.",
            request_result.value().request_id);

    std::cout
        << pyrelune::serialize_response(response)
        << '\n';

    return 1;
  }

  pyrelune::Kernel kernel{
      pyrelune::KernelOptions{
          .python_executable =
              resolve_python_executable(),
          .runtime_script = runtime_script,
          .timeout = std::chrono::seconds{30}}};

  auto response_result =
      kernel.execute(request_result.value());

  if (!response_result)
  {
    const auto response =
        error_response(
            response_result.error().message(),
            request_result.value().request_id);

    std::cout
        << pyrelune::serialize_response(response)
        << '\n';

    return 1;
  }

  const auto &response = response_result.value();

  std::cout
      << pyrelune::serialize_response(response)
      << '\n';

  return response.ok ? 0 : 1;
}
