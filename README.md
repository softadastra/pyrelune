# Pyrelune

Pyrelune is a Python execution kernel for interactive notebooks in Vix Note.

It allows Vix Note to execute Python cells through the `vix-note-extension-1` protocol while capturing standard output, standard error, execution failures, diagnostics, and rich MIME outputs.

## Features

- Execute Python cells from Vix Note
- Capture `stdout` and `stderr`
- Return structured Python errors
- Report syntax and runtime diagnostics
- Display the value of the final Python expression
- Support configurable Python executables
- Support custom working directories
- Enforce execution timeouts
- Install as a standard Vix package
- Integrate with the Vix Note extension registry
- Use a lightweight one-shot runtime without external C++ dependencies

## Requirements

Pyrelune requires:

- C++20 compiler
- CMake 3.20 or newer
- Python 3
- POSIX-compatible operating system for the current process runner

The initial process implementation supports Linux and macOS.

Windows process execution is not yet implemented.

## Installation with Vix

Install Pyrelune globally:

```bash
vix install -g softadastra/pyrelune
```

The global Vix binary directory must be available on `PATH`:

```bash
export PATH="$HOME/.vix/global/bin:$PATH"
```

Verify the installation:

```bash
command -v pyrelune
```

Vix Note can then discover the extension from the global package installation.

## Build from source

Clone the repository:

```bash
git clone https://github.com/softadastra/pyrelune.git
cd pyrelune
```

Configure the project:

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release
```

Build:

```bash
cmake --build build -j
```

Run the tests:

```bash
ctest --test-dir build \
  --output-on-failure
```

## Build with Vix

Configure and build using Vix:

```bash
vix build
```

Build every available target:

```bash
vix build --build-target all
```

Run the test targets:

```bash
vix test
```

## Install from source

Install into the default CMake prefix:

```bash
sudo cmake --install build
```

Install into a custom prefix:

```bash
cmake --install build \
  --prefix "$HOME/.local"
```

The installation contains approximately:

```text
bin/
└── pyrelune

include/
└── pyrelune/
    ├── error.hpp
    ├── kernel.hpp
    ├── protocol.hpp
    ├── pyrelune.hpp
    ├── python_process.hpp
    ├── request.hpp
    ├── response.hpp
    ├── result.hpp
    └── version.hpp

lib/
└── libpyrelune.*

share/
└── pyrelune/
    └── pyrelune-runtime.py
```

## Vix Note extension

Pyrelune is declared as a standard Vix package with a Note extension:

```json
{
  "extensions": {
    "note": {
      "api": "vix-note-extension-1",
      "runtime": {
        "command": "pyrelune",
        "mode": "oneshot"
      },
      "cellTypes": [
        {
          "id": "python",
          "label": "Python",
          "language": "python",
          "executable": true,
          "aliases": ["py"]
        }
      ]
    }
  }
}
```

After installation, Vix Note should expose `Python` as an available cell type.

Launch Vix Note:

```bash
vix note
```

List discovered extensions:

```bash
vix note --list-extensions
```

## Protocol

Pyrelune implements:

```text
vix-note-extension-1
```

It uses a one-shot process model:

```text
Vix Note
    ↓
starts pyrelune
    ↓
writes one JSON request to stdin
    ↓
Pyrelune executes Python
    ↓
writes one JSON response to stdout
    ↓
process exits
```

The full protocol is documented in:

```text
docs/protocol.md
```

## Manual execution

Send a request directly to the runtime:

```bash
printf '%s' '{
  "protocol": "vix-note-extension-1",
  "requestId": "manual-1",
  "cellId": "cell-1",
  "source": "print(2 + 3)",
  "workingDirectory": ""
}' | ./build/pyrelune
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

## Final expression output

Pyrelune displays the value of the final Python expression.

Example:

```python
value = 10
value * 2
```

The final expression produces a MIME output similar to:

```json
{
  "mime": "text/plain",
  "data": "20"
}
```

Expressions that evaluate to `None` do not produce a rich output.

## Standard output

Python output written with `print()` is captured separately:

```python
print("Hello from Pyrelune")
```

Response:

```json
{
  "stdout": "Hello from Pyrelune\n"
}
```

## Standard error

Text written to standard error is also captured:

```python
import sys

print("Something happened", file=sys.stderr)
```

Response:

```json
{
  "stderr": "Something happened\n"
}
```

## Diagnostics

Python exceptions produce structured diagnostics.

Example source:

```python
def greet(name)
    print(name)
```

Possible diagnostic:

```json
{
  "severity": "error",
  "message": "expected ':'",
  "code": "SyntaxError",
  "line": 1,
  "column": 16
}
```

Runtime exceptions are also returned:

```python
raise ValueError("invalid value")
```

## Working directory

Vix Note can provide a working directory with each request:

```json
{
  "workingDirectory": "/home/user/project"
}
```

Python code then executes from that directory:

```python
import os

print(os.getcwd())
```

## Environment variables

### `PYRELUNE_PYTHON`

Overrides the Python executable used by Pyrelune.

```bash
PYRELUNE_PYTHON=/usr/bin/python3.13 pyrelune
```

This can also point to a virtual environment:

```bash
PYRELUNE_PYTHON="$HOME/project/.venv/bin/python" pyrelune
```

### `PYRELUNE_RUNTIME_SCRIPT`

Overrides the Python runtime script path:

```bash
PYRELUNE_RUNTIME_SCRIPT=/opt/pyrelune/pyrelune-runtime.py \
  pyrelune
```

This is mainly useful during development and testing.

### `PYRELUNE_TEST_PYTHON`

Overrides the Python executable used by the test suite:

```bash
PYRELUNE_TEST_PYTHON=/usr/bin/python3 \
  ctest --test-dir build --output-on-failure
```

## Public C++ API

The complete public API is available through:

```cpp
#include <pyrelune/pyrelune.hpp>
```

Individual components can also be included directly:

```cpp
#include <pyrelune/kernel.hpp>
#include <pyrelune/protocol.hpp>
#include <pyrelune/python_process.hpp>
```

Example:

```cpp
#include <pyrelune/kernel.hpp>
#include <pyrelune/version.hpp>

#include <chrono>
#include <filesystem>

int main()
{
    pyrelune::Kernel kernel{
        pyrelune::KernelOptions{
            .python_executable = "python3",
            .runtime_script =
                std::filesystem::path{
                    "/usr/local/share/pyrelune/"
                    "pyrelune-runtime.py"},
            .timeout = std::chrono::seconds{30}}};

    pyrelune::Request request{
        .protocol =
            std::string{pyrelune::note_extension_api},
        .request_id = "request-1",
        .cell_id = "cell-1",
        .source = "print(2 + 3)",
        .working_directory = {}};

    auto result = kernel.execute(request);

    return result && result.value().ok ? 0 : 1;
}
```

## Project structure

```text
pyrelune/
├── CMakeLists.txt
├── LICENSE
├── README.md
├── vix.json
│
├── include/
│   └── pyrelune/
│       ├── error.hpp
│       ├── kernel.hpp
│       ├── protocol.hpp
│       ├── pyrelune.hpp
│       ├── python_process.hpp
│       ├── request.hpp
│       ├── response.hpp
│       ├── result.hpp
│       └── version.hpp
│
├── src/
│   ├── kernel.cpp
│   ├── main.cpp
│   ├── protocol.cpp
│   └── python_process.cpp
│
├── scripts/
│   └── pyrelune-runtime.py
│
├── tests/
│   ├── kernel_test.cpp
│   └── protocol_test.cpp
│
├── examples/
│   └── hello.py
│
└── docs/
    └── protocol.md
```

## Testing

The protocol test verifies:

- valid request parsing
- optional working directories
- escaped multiline source
- protocol validation
- required fields
- invalid JSON input
- response serialization
- MIME outputs
- diagnostic serialization

The kernel test verifies:

- successful Python execution
- standard output capture
- Python exceptions
- request validation
- missing runtime scripts
- working-directory handling

Run all tests:

```bash
ctest --test-dir build \
  --output-on-failure
```

Run a specific test:

```bash
ctest --test-dir build \
  -R pyrelune.protocol \
  --output-on-failure
```

```bash
ctest --test-dir build \
  -R pyrelune.kernel \
  --output-on-failure
```

## Security

Pyrelune executes arbitrary Python code.

Only execute trusted notebooks and trusted source code.

The current implementation does not provide:

- Python sandboxing
- filesystem isolation
- network isolation
- memory limits
- package restrictions
- system-call filtering
- operating-system user isolation

For untrusted execution, run Pyrelune inside a dedicated security boundary such as:

- container
- restricted operating-system user
- virtual machine
- application sandbox
- process isolation environment

Pyrelune should not be treated as a security sandbox.

## Current limitations

The initial release uses a one-shot Python process for every execution.

It does not yet provide:

- persistent state between cells
- interrupt support
- streaming outputs
- interactive standard input
- debugger integration
- package environment management
- rich Python display hooks
- asynchronous execution events
- Windows child-process execution

## Roadmap

Planned future work includes:

- persistent Python kernels
- state shared between cells
- execution interruption
- streaming output events
- HTML and JSON display hooks
- image outputs
- virtual environment discovery
- Python dependency information
- Windows process support
- stronger resource limits
- richer diagnostics
- kernel lifecycle controls

## License

Pyrelune is licensed under the MIT License.

See [LICENSE](LICENSE) for details.

## Softadastra

Pyrelune is developed and maintained by Softadastra.

Softadastra builds modern tooling for C++ development and native applications.
