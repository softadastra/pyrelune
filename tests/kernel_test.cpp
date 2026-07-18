/**
 * @file kernel_test.cpp
 * @brief Tests the Pyrelune Python execution kernel.
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
#include <pyrelune/version.hpp>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>

namespace
{
  int failures = 0;

  void expect(
      bool condition,
      std::string_view message)
  {
    if (condition)
    {
      return;
    }

    std::cerr << "FAILED: " << message << '\n';
    ++failures;
  }

  class TemporaryRuntime
  {
  public:
    TemporaryRuntime()
    {
      const auto timestamp =
          std::chrono::steady_clock::now()
              .time_since_epoch()
              .count();

      directory_ =
          std::filesystem::temp_directory_path() /
          ("pyrelune-kernel-test-" +
           std::to_string(timestamp));

      std::filesystem::create_directories(directory_);

      script_ = directory_ / "runtime.py";

      std::ofstream output{script_};

      output << R"PYTHON(
import contextlib
import io
import json
import sys
import traceback

request = json.loads(sys.stdin.read())

stdout_buffer = io.StringIO()
stderr_buffer = io.StringIO()

response = {
    "ok": False,
    "requestId": request.get("requestId", ""),
    "stdout": "",
    "stderr": "",
    "error": "",
    "outputs": [],
    "diagnostics": []
}

try:
    namespace = {}

    with contextlib.redirect_stdout(stdout_buffer):
        with contextlib.redirect_stderr(stderr_buffer):
            exec(request.get("source", ""), namespace, namespace)

    response["ok"] = True
except Exception as error:
    response["error"] = "".join(
        traceback.format_exception(
            type(error),
            error,
            error.__traceback__
        )
    )
    response["diagnostics"].append({
        "severity": "error",
        "message": str(error),
        "code": type(error).__name__,
        "line": 0,
        "column": 0
    })

response["stdout"] = stdout_buffer.getvalue()
response["stderr"] = stderr_buffer.getvalue()

sys.stdout.write(json.dumps(response, separators=(",", ":")))
sys.stdout.write("\n")
)PYTHON";
    }

    TemporaryRuntime(const TemporaryRuntime &) = delete;
    TemporaryRuntime &operator=(const TemporaryRuntime &) = delete;

    ~TemporaryRuntime()
    {
      std::error_code error;
      std::filesystem::remove_all(directory_, error);
    }

    [[nodiscard]] const std::filesystem::path &script() const
        noexcept
    {
      return script_;
    }

  private:
    std::filesystem::path directory_;
    std::filesystem::path script_;
  };

  [[nodiscard]] std::filesystem::path python_executable()
  {
    if (const char *configured =
            std::getenv("PYRELUNE_TEST_PYTHON"))
    {
      if (*configured != '\0')
      {
        return configured;
      }
    }

    return "python3";
  }

  [[nodiscard]] pyrelune::Kernel create_kernel(
      const std::filesystem::path &runtime_script,
      std::chrono::milliseconds timeout =
          std::chrono::seconds{10})
  {
    return pyrelune::Kernel{
        pyrelune::KernelOptions{
            .python_executable = python_executable(),
            .runtime_script = runtime_script,
            .timeout = timeout}};
  }

  [[nodiscard]] pyrelune::Request make_request(
      std::string source,
      std::string request_id = "request-1")
  {
    return pyrelune::Request{
        .protocol =
            std::string{pyrelune::note_extension_api},
        .request_id = std::move(request_id),
        .cell_id = "cell-1",
        .source = std::move(source),
        .working_directory = {}};
  }

  void test_successful_execution(
      const TemporaryRuntime &runtime)
  {
    auto kernel = create_kernel(runtime.script());

    auto result = kernel.execute(
        make_request("print(2 + 3)"));

    expect(
        result.has_value(),
        "Successful Python execution should return a response");

    if (!result)
    {
      return;
    }

    const auto &response = result.value();

    expect(
        response.ok,
        "The execution response should be successful");

    expect(
        response.request_id == "request-1",
        "The request identifier should be preserved");

    expect(
        response.stdout_text == "5\n",
        "Python stdout should be captured");

    expect(
        response.stderr_text.empty(),
        "Successful execution should not produce stderr");

    expect(
        response.error.empty(),
        "Successful execution should not contain an error");
  }

  void test_python_exception(
      const TemporaryRuntime &runtime)
  {
    auto kernel = create_kernel(runtime.script());

    auto result = kernel.execute(
        make_request(
            "raise ValueError('bad value')",
            "request-error"));

    expect(
        result.has_value(),
        "A Python exception should produce a protocol response");

    if (!result)
    {
      return;
    }

    const auto &response = result.value();

    expect(
        !response.ok,
        "A Python exception should set ok=false");

    expect(
        response.request_id == "request-error",
        "The request identifier should be preserved on failure");

    expect(
        response.error.find("ValueError") !=
            std::string::npos,
        "The error should contain the exception type");

    expect(
        response.error.find("bad value") !=
            std::string::npos,
        "The error should contain the exception message");

    expect(
        !response.diagnostics.empty(),
        "A failed execution should contain a diagnostic");
  }

  void test_invalid_protocol(
      const TemporaryRuntime &runtime)
  {
    auto kernel = create_kernel(runtime.script());

    auto request = make_request("print(1)");
    request.protocol = "unsupported-protocol";

    auto result = kernel.execute(request);

    expect(
        result.has_error(),
        "An invalid protocol should fail before launching Python");

    if (!result)
    {
      expect(
          result.error().code() ==
              pyrelune::ErrorCode::invalid_protocol,
          "The failure should use invalid_protocol");
    }
  }

  void test_missing_request_id(
      const TemporaryRuntime &runtime)
  {
    auto kernel = create_kernel(runtime.script());

    auto request = make_request("print(1)");
    request.request_id.clear();

    auto result = kernel.execute(request);

    expect(
        result.has_error(),
        "An empty requestId should fail validation");

    if (!result)
    {
      expect(
          result.error().code() ==
              pyrelune::ErrorCode::missing_field,
          "The failure should use missing_field");
    }
  }

  void test_missing_runtime_script()
  {
    auto kernel = create_kernel(
        std::filesystem::temp_directory_path() /
        "pyrelune-runtime-does-not-exist.py");

    auto result = kernel.execute(
        make_request("print(1)"));

    expect(
        result.has_error(),
        "A missing runtime script should fail");

    if (!result)
    {
      expect(
          result.error().code() ==
              pyrelune::ErrorCode::io_error,
          "A missing runtime should use io_error");
    }
  }

  void test_working_directory(
      const TemporaryRuntime &runtime)
  {
    const auto working_directory =
        std::filesystem::temp_directory_path();

    auto kernel = create_kernel(runtime.script());

    auto request = make_request(
        "import os\nprint(os.getcwd())",
        "request-directory");

    request.working_directory =
        working_directory.string();

    auto result = kernel.execute(request);

    expect(
        result.has_value(),
        "Execution with a working directory should succeed");

    if (!result)
    {
      return;
    }

    std::error_code error;

    const auto expected =
        std::filesystem::weakly_canonical(
            working_directory,
            error);

    const auto actual_text =
        result.value().stdout_text;

    const auto actual =
        std::filesystem::weakly_canonical(
            std::filesystem::path{
                actual_text.substr(
                    0,
                    actual_text.find_last_not_of("\r\n") + 1)},
            error);

    expect(
        actual == expected,
        "Python should execute in the requested directory");
  }
}

int main()
{
  TemporaryRuntime runtime;

  test_successful_execution(runtime);
  test_python_exception(runtime);
  test_invalid_protocol(runtime);
  test_missing_request_id(runtime);
  test_missing_runtime_script();
  test_working_directory(runtime);

  if (failures != 0)
  {
    std::cerr
        << failures
        << " kernel test(s) failed\n";

    return EXIT_FAILURE;
  }

  std::cout << "All kernel tests passed\n";
  return EXIT_SUCCESS;
}
