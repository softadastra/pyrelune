/**
 * @file protocol_test.cpp
 * @brief Tests the Pyrelune Vix Note extension protocol.
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

#include <cstdlib>
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

  void test_parse_valid_request()
  {
    const std::string input = R"json({
      "protocol": "vix-note-extension-1",
      "requestId": "request-1",
      "cellId": "cell-1",
      "source": "print(\"Hello from Python\")",
      "workingDirectory": "/tmp"
    })json";

    const auto result = pyrelune::parse_request(input);

    expect(
        result.has_value(),
        "A valid request should be parsed");

    if (!result)
    {
      return;
    }

    const auto &request = result.value();

    expect(
        request.protocol == pyrelune::note_extension_api,
        "The protocol should be preserved");

    expect(
        request.request_id == "request-1",
        "The request identifier should be parsed");

    expect(
        request.cell_id == "cell-1",
        "The cell identifier should be parsed");

    expect(
        request.source == "print(\"Hello from Python\")",
        "The Python source should be unescaped");

    expect(
        request.working_directory == "/tmp",
        "The working directory should be parsed");
  }

  void test_optional_working_directory()
  {
    const std::string input = R"json({
      "protocol": "vix-note-extension-1",
      "requestId": "request-2",
      "cellId": "cell-2",
      "source": "2 + 3"
    })json";

    const auto result = pyrelune::parse_request(input);

    expect(
        result.has_value(),
        "workingDirectory should be optional");

    if (result)
    {
      expect(
          result.value().working_directory.empty(),
          "Missing workingDirectory should produce an empty value");
    }
  }

  void test_multiline_source()
  {
    const std::string input = R"json({
      "protocol": "vix-note-extension-1",
      "requestId": "request-3",
      "cellId": "cell-3",
      "source": "value = 5\nprint(value)\n"
    })json";

    const auto result = pyrelune::parse_request(input);

    expect(
        result.has_value(),
        "A request containing multiline source should be parsed");

    if (result)
    {
      expect(
          result.value().source ==
              "value = 5\nprint(value)\n",
          "Escaped newlines should be decoded");
    }
  }

  void test_invalid_protocol()
  {
    const std::string input = R"json({
      "protocol": "unsupported-protocol",
      "requestId": "request-4",
      "cellId": "cell-4",
      "source": "print(1)"
    })json";

    const auto result = pyrelune::parse_request(input);

    expect(
        result.has_error(),
        "An unsupported protocol should fail");

    if (!result)
    {
      expect(
          result.error().code() ==
              pyrelune::ErrorCode::invalid_protocol,
          "The error should be invalid_protocol");
    }
  }

  void test_missing_required_field()
  {
    const std::string input = R"json({
      "protocol": "vix-note-extension-1",
      "requestId": "request-5",
      "source": "print(1)"
    })json";

    const auto result = pyrelune::parse_request(input);

    expect(
        result.has_error(),
        "A request without cellId should fail");

    if (!result)
    {
      expect(
          result.error().code() ==
              pyrelune::ErrorCode::missing_field,
          "The error should be missing_field");
    }
  }

  void test_invalid_json()
  {
    const auto result = pyrelune::parse_request(
        R"json(["not", "an", "object"])json");

    expect(
        result.has_error(),
        "A JSON array should not be accepted as a request");

    if (!result)
    {
      expect(
          result.error().code() ==
              pyrelune::ErrorCode::invalid_json,
          "The error should be invalid_json");
    }
  }

  void test_serialize_success_response()
  {
    const pyrelune::Response response{
        .ok = true,
        .request_id = "request-6",
        .stdout_text = "Hello\n",
        .stderr_text = "",
        .error = "",
        .outputs = {
            pyrelune::Output{
                .mime = "text/plain",
                .data = "5"}},
        .diagnostics = {}};

    const std::string json =
        pyrelune::serialize_response(response);

    expect(
        json.find("\"ok\":true") != std::string::npos,
        "The serialized response should contain ok=true");

    expect(
        json.find("\"requestId\":\"request-6\"") !=
            std::string::npos,
        "The request identifier should be serialized");

    expect(
        json.find("\"stdout\":\"Hello\\n\"") !=
            std::string::npos,
        "Newlines should be escaped");

    expect(
        json.find("\"mime\":\"text/plain\"") !=
            std::string::npos,
        "MIME outputs should be serialized");

    expect(
        json.find("\"data\":\"5\"") !=
            std::string::npos,
        "MIME output data should be serialized");
  }

  void test_serialize_error_response()
  {
    const pyrelune::Response response{
        .ok = false,
        .request_id = "request-7",
        .stdout_text = "",
        .stderr_text = "",
        .error = "invalid \"syntax\"",
        .outputs = {},
        .diagnostics = {
            pyrelune::Diagnostic{
                .severity = "error",
                .message = "invalid syntax",
                .code = "SyntaxError",
                .line = 3,
                .column = 8}}};

    const std::string json =
        pyrelune::serialize_response(response);

    expect(
        json.find("\"ok\":false") != std::string::npos,
        "The serialized response should contain ok=false");

    expect(
        json.find("invalid \\\"syntax\\\"") !=
            std::string::npos,
        "Quotation marks should be escaped");

    expect(
        json.find("\"code\":\"SyntaxError\"") !=
            std::string::npos,
        "The diagnostic code should be serialized");

    expect(
        json.find("\"line\":3") !=
            std::string::npos,
        "The diagnostic line should be serialized");

    expect(
        json.find("\"column\":8") !=
            std::string::npos,
        "The diagnostic column should be serialized");
  }
}

int main()
{
  test_parse_valid_request();
  test_optional_working_directory();
  test_multiline_source();
  test_invalid_protocol();
  test_missing_required_field();
  test_invalid_json();
  test_serialize_success_response();
  test_serialize_error_response();

  if (failures != 0)
  {
    std::cerr
        << failures
        << " protocol test(s) failed\n";

    return EXIT_FAILURE;
  }

  std::cout << "All protocol tests passed\n";
  return EXIT_SUCCESS;
}
