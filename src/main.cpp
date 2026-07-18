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

#if defined(__linux__)
#include <unistd.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <filesystem>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace
{
  struct RuntimeScriptResolution
  {
    std::filesystem::path path;
    std::vector<std::filesystem::path> checked;
  };

  [[nodiscard]] bool file_exists(const std::filesystem::path &path)
  {
    std::error_code error;
    return std::filesystem::is_regular_file(path, error);
  }

  [[nodiscard]] std::filesystem::path normalize_existing_candidate(
      const std::filesystem::path &candidate)
  {
    std::error_code error;
    const auto canonical =
        std::filesystem::weakly_canonical(candidate, error);

    if (!error && file_exists(canonical))
    {
      return canonical;
    }

    if (file_exists(candidate))
    {
      return candidate;
    }

    return {};
  }

  void add_candidate(
      std::vector<std::filesystem::path> &candidates,
      const std::filesystem::path &candidate)
  {
    if (candidate.empty())
    {
      return;
    }

    candidates.push_back(candidate);
  }

  [[nodiscard]] std::filesystem::path current_executable_path()
  {
#if defined(__linux__)
    std::vector<char> buffer(4096);

    for (;;)
    {
      const ssize_t size =
          ::readlink(
              "/proc/self/exe",
              buffer.data(),
              buffer.size() - 1);

      if (size < 0)
      {
        return {};
      }

      if (static_cast<std::size_t>(size) < buffer.size() - 1)
      {
        buffer[static_cast<std::size_t>(size)] = '\0';
        return std::filesystem::path{buffer.data()};
      }

      buffer.resize(buffer.size() * 2);
    }
#elif defined(__APPLE__)
    std::uint32_t size = 0;
    (void)::_NSGetExecutablePath(nullptr, &size);

    if (size == 0)
    {
      return {};
    }

    std::vector<char> buffer(size + 1);

    if (::_NSGetExecutablePath(buffer.data(), &size) != 0)
    {
      return {};
    }

    buffer[size] = '\0';
    return std::filesystem::path{buffer.data()};
#elif defined(_WIN32)
    std::wstring buffer(4096, L'\0');

    for (;;)
    {
      const DWORD size =
          ::GetModuleFileNameW(
              nullptr,
              buffer.data(),
              static_cast<DWORD>(buffer.size()));

      if (size == 0)
      {
        return {};
      }

      if (size < buffer.size())
      {
        buffer.resize(size);
        return std::filesystem::path{buffer};
      }

      buffer.resize(buffer.size() * 2);
    }
#else
    return {};
#endif
  }

  [[nodiscard]] RuntimeScriptResolution resolve_runtime_script(
      const std::filesystem::path &argv_executable)
  {
    RuntimeScriptResolution resolution;

    if (const char *configured =
            std::getenv("PYRELUNE_RUNTIME_SCRIPT"))
    {
      if (*configured != '\0')
      {
        const std::filesystem::path configured_path{configured};
        add_candidate(resolution.checked, configured_path);

        if (auto existing = normalize_existing_candidate(configured_path);
            !existing.empty())
        {
          resolution.path = existing;
          return resolution;
        }
      }
    }

    std::vector<std::filesystem::path> executable_candidates;

    add_candidate(executable_candidates, current_executable_path());

    if (!argv_executable.empty())
    {
      add_candidate(executable_candidates, argv_executable);

      if (argv_executable.has_parent_path())
      {
        std::error_code error;
        add_candidate(
            executable_candidates,
            std::filesystem::weakly_canonical(argv_executable, error));
      }
    }

    for (const auto &executable : executable_candidates)
    {
      if (executable.empty())
      {
        continue;
      }

      const auto directory = executable.parent_path();

      if (directory.empty())
      {
        continue;
      }

      add_candidate(
          resolution.checked,
          directory / "../share/pyrelune/pyrelune-runtime.py");
      add_candidate(
          resolution.checked,
          directory / "../share/pyrelune/scripts/pyrelune-runtime.py");
      add_candidate(
          resolution.checked,
          directory / "../scripts/pyrelune-runtime.py");
      add_candidate(
          resolution.checked,
          directory / "scripts/pyrelune-runtime.py");
    }

#if defined(PYRELUNE_SOURCE_RUNTIME_SCRIPT)
    add_candidate(
        resolution.checked,
        std::filesystem::path{PYRELUNE_SOURCE_RUNTIME_SCRIPT});
#endif

#if defined(PYRELUNE_INSTALL_RUNTIME_SCRIPT)
    add_candidate(
        resolution.checked,
        std::filesystem::path{PYRELUNE_INSTALL_RUNTIME_SCRIPT});
#endif

    add_candidate(
        resolution.checked,
        std::filesystem::current_path() / "scripts/pyrelune-runtime.py");

    for (const auto &candidate : resolution.checked)
    {
      if (auto existing = normalize_existing_candidate(candidate);
          !existing.empty())
      {
        resolution.path = existing;
        return resolution;
      }
    }

    return resolution;
  }

  [[nodiscard]] std::string runtime_script_error_message(
      const RuntimeScriptResolution &resolution)
  {
    std::string message =
        "Pyrelune runtime script was not found.\n"
        "Set PYRELUNE_RUNTIME_SCRIPT or reinstall Pyrelune.\n\n"
        "Checked:";

    for (const auto &candidate : resolution.checked)
    {
      message += "\n  ";
      message += candidate.string();
    }

    return message;
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

  const auto runtime_script_resolution =
      resolve_runtime_script(executable);

  if (runtime_script_resolution.path.empty())
  {
    const auto response =
        error_response(
            runtime_script_error_message(runtime_script_resolution),
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
          .runtime_script = runtime_script_resolution.path,
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
