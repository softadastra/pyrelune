/**
 * @file python_process.hpp
 * @brief Declares the Python subprocess runner used by Pyrelune.
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

#include <pyrelune/result.hpp>

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

namespace pyrelune
{
  /**
   * @brief Contains the captured result of a child process.
   */
  struct ProcessResult
  {
    /**
     * @brief Child process exit code.
     *
     * A negative value indicates that the process did not exit normally.
     */
    int exit_code{-1};

    /**
     * @brief Complete text captured from standard output.
     */
    std::string stdout_text;

    /**
     * @brief Complete text captured from standard error.
     */
    std::string stderr_text;

    /**
     * @brief Indicates whether the process exceeded its timeout.
     */
    bool timed_out{false};
  };

  /**
   * @brief Configuration used to launch the Python runtime.
   */
  struct PythonProcessOptions
  {
    /**
     * @brief Python executable name or absolute path.
     */
    std::filesystem::path executable{"python3"};

    /**
     * @brief Python script implementing the Pyrelune runtime.
     */
    std::filesystem::path runtime_script;

    /**
     * @brief Optional working directory for the child process.
     */
    std::filesystem::path working_directory;

    /**
     * @brief Maximum execution duration.
     */
    std::chrono::milliseconds timeout{
        std::chrono::seconds{30}};

    /**
     * @brief Additional arguments passed before the runtime script.
     */
    std::vector<std::string> arguments;
  };

  /**
   * @brief Executes the Python runtime as a child process.
   *
   * The input is written to the child process standard input. Standard
   * output and standard error are captured independently.
   *
   * @param options Process launch configuration.
   * @param input Complete protocol request written to standard input.
   * @return Captured process result or a structured launch error.
   */
  [[nodiscard]] Result<ProcessResult> run_python_process(
      const PythonProcessOptions &options,
      const std::string &input);
}
