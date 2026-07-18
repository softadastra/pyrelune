#!/usr/bin/env python3

"""
Pyrelune Python runtime.

Reads one Vix Note extension request from standard input, executes the Python
source, and writes one JSON response to standard output.

Copyright (c) 2026 Softadastra.
Licensed under the MIT License.
"""

from __future__ import annotations

import ast
import contextlib
import io
import json
import os
import sys
import traceback
from dataclasses import dataclass
from typing import Any


PROTOCOL = "vix-note-extension-1"


@dataclass
class RuntimeRequest:
    """Execution request received from Pyrelune."""

    protocol: str
    request_id: str
    cell_id: str
    source: str
    working_directory: str


def read_request() -> RuntimeRequest:
    """Read and validate one request from standard input."""

    raw_input = sys.stdin.read()

    if not raw_input.strip():
        raise ValueError("The runtime request is empty")

    try:
        payload = json.loads(raw_input)
    except json.JSONDecodeError as error:
        raise ValueError(f"Invalid JSON request: {error}") from error

    if not isinstance(payload, dict):
        raise ValueError("The runtime request must be a JSON object")

    protocol = payload.get("protocol")

    if protocol != PROTOCOL:
        raise ValueError(
            f"Unsupported protocol: {protocol!r}. Expected {PROTOCOL!r}"
        )

    request_id = payload.get("requestId")
    cell_id = payload.get("cellId")
    source = payload.get("source")
    working_directory = payload.get("workingDirectory", "")

    if not isinstance(request_id, str) or not request_id:
        raise ValueError("Missing or invalid requestId")

    if not isinstance(cell_id, str) or not cell_id:
        raise ValueError("Missing or invalid cellId")

    if not isinstance(source, str):
        raise ValueError("Missing or invalid source")

    if not isinstance(working_directory, str):
        raise ValueError("workingDirectory must be a string")

    return RuntimeRequest(
        protocol=protocol,
        request_id=request_id,
        cell_id=cell_id,
        source=source,
        working_directory=working_directory,
    )


def diagnostic_from_exception(error: BaseException) -> dict[str, Any]:
    """Create a structured diagnostic from a Python exception."""

    line = 0
    column = 0

    if isinstance(error, SyntaxError):
        line = error.lineno or 0
        column = error.offset or 0
        message = error.msg
    else:
        message = str(error) or error.__class__.__name__

        extracted = traceback.extract_tb(error.__traceback__)

        if extracted:
            frame = extracted[-1]
            line = frame.lineno

    return {
        "severity": "error",
        "message": message,
        "code": error.__class__.__name__,
        "line": line,
        "column": column,
    }


def output_from_value(value: Any) -> dict[str, str] | None:
    """Convert a final expression value into a MIME output."""

    if value is None:
        return None

    if isinstance(value, str):
        return {
            "mime": "text/plain",
            "data": value,
        }

    try:
        rendered = repr(value)
    except Exception:
        rendered = f"<{type(value).__name__}>"

    return {
        "mime": "text/plain",
        "data": rendered,
    }


def execute_source(source: str) -> tuple[Any, dict[str, Any]]:
    """
    Execute Python source and return the final expression value when present.

    The final expression is evaluated separately, providing notebook-like
    behavior where a value such as `2 + 3` produces a visible output.
    """

    module = ast.parse(source, filename="<pyrelune>", mode="exec")
    namespace: dict[str, Any] = {
        "__name__": "__pyrelune__",
        "__builtins__": __builtins__,
    }

    final_expression: ast.expr | None = None

    if module.body and isinstance(module.body[-1], ast.Expr):
        final_expression = module.body.pop().value

    if module.body:
        executable = compile(
            module,
            filename="<pyrelune>",
            mode="exec",
        )
        exec(executable, namespace, namespace)

    value: Any = None

    if final_expression is not None:
        expression = ast.Expression(body=final_expression)
        ast.fix_missing_locations(expression)

        compiled_expression = compile(
            expression,
            filename="<pyrelune>",
            mode="eval",
        )

        value = eval(compiled_expression, namespace, namespace)

    return value, namespace


def execute_request(request: RuntimeRequest) -> dict[str, Any]:
    """Execute one validated runtime request."""

    stdout_buffer = io.StringIO()
    stderr_buffer = io.StringIO()

    original_directory = os.getcwd()
    changed_directory = False

    try:
        if request.working_directory:
            os.chdir(request.working_directory)
            changed_directory = True

        with contextlib.redirect_stdout(stdout_buffer):
            with contextlib.redirect_stderr(stderr_buffer):
                value, _namespace = execute_source(request.source)

        outputs: list[dict[str, str]] = []
        value_output = output_from_value(value)

        if value_output is not None:
            outputs.append(value_output)

        return {
            "ok": True,
            "requestId": request.request_id,
            "stdout": stdout_buffer.getvalue(),
            "stderr": stderr_buffer.getvalue(),
            "error": "",
            "outputs": outputs,
            "diagnostics": [],
        }

    except BaseException as error:
        return {
            "ok": False,
            "requestId": request.request_id,
            "stdout": stdout_buffer.getvalue(),
            "stderr": stderr_buffer.getvalue(),
            "error": "".join(
                traceback.format_exception(
                    type(error),
                    error,
                    error.__traceback__,
                )
            ),
            "outputs": [],
            "diagnostics": [
                diagnostic_from_exception(error),
            ],
        }

    finally:
        if changed_directory:
            os.chdir(original_directory)


def error_response(message: str, request_id: str = "") -> dict[str, Any]:
    """Create a protocol-level failure response."""

    return {
        "ok": False,
        "requestId": request_id,
        "stdout": "",
        "stderr": "",
        "error": message,
        "outputs": [],
        "diagnostics": [
            {
                "severity": "error",
                "message": message,
                "code": "RuntimeProtocolError",
                "line": 0,
                "column": 0,
            }
        ],
    }


def write_response(response: dict[str, Any]) -> None:
    """Write exactly one compact JSON response to standard output."""

    sys.stdout.write(
        json.dumps(
            response,
            ensure_ascii=False,
            separators=(",", ":"),
        )
    )
    sys.stdout.write("\n")
    sys.stdout.flush()


def main() -> int:
    """Run one Pyrelune one-shot execution."""

    try:
        request = read_request()
        response = execute_request(request)
    except Exception as error:
        response = error_response(str(error))

    write_response(response)

    return 0 if response.get("ok") else 1


if __name__ == "__main__":
    raise SystemExit(main())
