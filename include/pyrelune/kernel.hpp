/**
 * @file kernel.hpp
 * @brief Declares the Pyrelune Python execution kernel.
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

#include <pyrelune/python_process.hpp>
#include <pyrelune/request.hpp>
#include <pyrelune/response.hpp>
#include <pyrelune/result.hpp>

#include <chrono>
#include <filesystem>
#include <string>

namespace pyrelune
{
  /**
   * @brief Configuration used by the Pyrelune execution kernel.
   */
  struct KernelOptions
  {
    /**
     * @brief Python executable name or absolute path.
     */
    std::filesystem::path python_executable{"python3"};

    /**
     * @brief Path to the Python runtime script.
     */
    std::filesystem::path runtime_script;

    /**
     * @brief Maximum duration of one Python execution.
     */
    std::chrono::milliseconds timeout{
        std::chrono::seconds{30}};
  };

  /**
   * @brief Executes Python cells through the Pyrelune runtime.
   */
  class Kernel
  {
  public:
    /**
     * @brief Constructs a kernel with the provided configuration.
     *
     * @param options Kernel configuration.
     */
    explicit Kernel(KernelOptions options);

    /**
     * @brief Executes one Vix Note request.
     *
     * @param request Validated execution request.
     * @return Structured response or an internal runtime error.
     */
    [[nodiscard]] Result<Response> execute(
        const Request &request) const;

    /**
     * @brief Returns the current kernel configuration.
     */
    [[nodiscard]] const KernelOptions &options() const noexcept;

  private:
    KernelOptions options_;
  };
}
