# Pyrelune Protocol

Pyrelune implements the Vix Note extension protocol for executing Python cells.

The current protocol identifier is:

```text
vix-note-extension-1
```

Pyrelune currently uses a one-shot process model:

```text
Vix Note
   ↓
starts pyrelune
   ↓
writes one JSON request to stdin
   ↓
Pyrelune executes the Python source
   ↓
writes one JSON response to stdout
   ↓
process exits
```

## Transport

Requests and responses are exchanged through standard input and standard output.

Pyrelune reads the complete request from `stdin` and writes exactly one JSON response to `stdout`.

Diagnostic or launcher messages must not be written to `stdout`, because Vix Note expects valid JSON there.

Process-level diagnostic messages may be written to `stderr`.

## Request

A request is a JSON object with the following fields:

| Field              |   Type | Required | Description                                           |
| ------------------ | -----: | -------: | ----------------------------------------------------- |
| `protocol`         | string |      yes | Protocol identifier. Must be `vix-note-extension-1`.  |
| `requestId`        | string |      yes | Identifier used to correlate the response.            |
| `cellId`           | string |      yes | Identifier of the Vix Note cell being executed.       |
| `source`           | string |      yes | Python source code to execute.                        |
| `workingDirectory` | string |       no | Directory in which the Python runtime should execute. |

Example:

```json
{
  "protocol": "vix-note-extension-1",
  "requestId": "request-1",
  "cellId": "cell-1",
  "source": "print(2 + 3)",
  "workingDirectory": "/tmp/project"
}
```

An omitted `workingDirectory` is treated as an empty string.

## Response

A response is a JSON object with the following fields:

| Field         |    Type | Description                                             |
| ------------- | ------: | ------------------------------------------------------- |
| `ok`          | boolean | Indicates whether execution succeeded.                  |
| `requestId`   |  string | Identifier copied from the request.                     |
| `stdout`      |  string | Text captured from standard output.                     |
| `stderr`      |  string | Text captured from standard error.                      |
| `error`       |  string | Human-readable execution error or empty string.         |
| `outputs`     |   array | Rich MIME outputs produced by the cell.                 |
| `diagnostics` |   array | Structured errors, warnings, or informational messages. |

Successful example:

```json
{
  "ok": true,
  "requestId": "request-1",
  "stdout": "5\n",
  "stderr": "",
  "error": "",
  "outputs": [],
  "diagnostics": []
}
```

Failed example:

```json
{
  "ok": false,
  "requestId": "request-2",
  "stdout": "",
  "stderr": "",
  "error": "Traceback (most recent call last):\n...",
  "outputs": [],
  "diagnostics": [
    {
      "severity": "error",
      "message": "invalid syntax",
      "code": "SyntaxError",
      "line": 1,
      "column": 7
    }
  ]
}
```

## MIME outputs

Each item in `outputs` contains:

| Field  |   Type | Description                          |
| ------ | -----: | ------------------------------------ |
| `mime` | string | MIME type used to render the output. |
| `data` | string | Serialized output content.           |

Example:

```json
{
  "mime": "text/plain",
  "data": "5"
}
```

Pyrelune initially supports `text/plain`.

Future versions may support additional output types such as:

```text
text/html
application/json
image/svg+xml
image/png
```

Binary data should be encoded appropriately for the selected MIME type. Base64 encoding is recommended for binary image formats.

## Diagnostics

Each diagnostic contains:

| Field      |    Type | Description                                       |
| ---------- | ------: | ------------------------------------------------- |
| `severity` |  string | `error`, `warning`, or `information`.             |
| `message`  |  string | Human-readable diagnostic message.                |
| `code`     |  string | Stable diagnostic or Python exception code.       |
| `line`     | integer | One-based source line, or `0` when unavailable.   |
| `column`   | integer | One-based source column, or `0` when unavailable. |

Example:

```json
{
  "severity": "error",
  "message": "expected ':'",
  "code": "SyntaxError",
  "line": 3,
  "column": 12
}
```

## Python execution behavior

Pyrelune executes the request source in an isolated namespace for each one-shot process.

Standard output and standard error are captured independently.

A final Python expression may be converted into a `text/plain` MIME output.

Example source:

```python
value = 10
value * 2
```

Possible response output:

```json
{
  "mime": "text/plain",
  "data": "20"
}
```

A final expression that evaluates to `None` does not produce a MIME output.

## Exit codes

Pyrelune uses the following process-level behavior:

| Exit code | Meaning                                              |
| --------: | ---------------------------------------------------- |
|       `0` | The Python cell executed successfully.               |
|       `1` | The request, runtime, or Python execution failed.    |
|     `126` | The runtime process could not be executed correctly. |
|     `127` | The configured Python executable was not found.      |

The JSON response remains the primary source of execution information.

Vix Note should inspect the response even when the process exits with a non-zero code.

## Environment variables

Pyrelune supports:

### `PYRELUNE_PYTHON`

Overrides the Python executable.

Example:

```bash
PYRELUNE_PYTHON=/usr/bin/python3.13 pyrelune
```

### `PYRELUNE_RUNTIME_SCRIPT`

Overrides the path to `pyrelune-runtime.py`.

Example:

```bash
PYRELUNE_RUNTIME_SCRIPT=/opt/pyrelune/pyrelune-runtime.py pyrelune
```

These variables are mainly useful for development, testing, and custom installations.

## Manual protocol test

Example request:

```bash
printf '%s' '{
  "protocol": "vix-note-extension-1",
  "requestId": "manual-1",
  "cellId": "cell-1",
  "source": "print(2 + 3)",
  "workingDirectory": ""
}' | pyrelune
```

Expected response:

```json
{
  "ok": true,
  "requestId": "manual-1",
  "stdout": "5\n",
  "stderr": "",
  "error": "",
  "outputs": [],
  "diagnostics": []
}
```

## Security considerations

Pyrelune executes arbitrary Python code.

It must only be used with trusted notebooks and trusted extension requests.

The current runtime does not provide:

- sandboxing;
- filesystem isolation;
- network isolation;
- package restrictions;
- operating-system permission isolation;
- memory limits;
- persistent namespace isolation between separate processes.

The configured working directory should be validated by the caller when requests may come from untrusted sources.

For stronger isolation, run Pyrelune inside a dedicated container, restricted user account, sandbox, or operating-system security boundary.

## Current limitations

The initial implementation uses one process per execution.

It does not yet provide:

- persistent Python state between cells;
- interrupt messages;
- streaming outputs;
- input prompts;
- debugger integration;
- package environment management;
- rich display hooks;
- asynchronous execution events;
- Windows process execution support.

These capabilities may be introduced through later protocol versions while preserving compatibility with `vix-note-extension-1`.
