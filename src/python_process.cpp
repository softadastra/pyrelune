/**
 * @file python_process.cpp
 * @brief Implements the Python subprocess runner used by Pyrelune.
 *
 * @author Softadastra
 *
 * @copyright
 * Copyright (c) 2026 Softadastra.
 *
 * @license
 * This project is licensed under the MIT License.
 */

#include <pyrelune/python_process.hpp>

#include <array>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#if defined(_WIN32)

namespace pyrelune
{
  Result<ProcessResult> run_python_process(
      const PythonProcessOptions &,
      const std::string &)
  {
    return Result<ProcessResult>::failure(
        Error{
            ErrorCode::process_failed,
            "Python process execution is not yet supported on Windows"});
  }
}

#else

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace pyrelune
{
  namespace
  {
    class FileDescriptor
    {
    public:
      explicit FileDescriptor(int value = -1) noexcept
          : value_(value)
      {
      }

      FileDescriptor(const FileDescriptor &) = delete;
      FileDescriptor &operator=(const FileDescriptor &) = delete;

      FileDescriptor(FileDescriptor &&other) noexcept
          : value_(std::exchange(other.value_, -1))
      {
      }

      FileDescriptor &operator=(FileDescriptor &&other) noexcept
      {
        if (this != &other)
        {
          reset();
          value_ = std::exchange(other.value_, -1);
        }

        return *this;
      }

      ~FileDescriptor()
      {
        reset();
      }

      [[nodiscard]] int get() const noexcept
      {
        return value_;
      }

      [[nodiscard]] bool valid() const noexcept
      {
        return value_ >= 0;
      }

      [[nodiscard]] int release() noexcept
      {
        return std::exchange(value_, -1);
      }

      void reset(int value = -1) noexcept
      {
        if (value_ >= 0)
        {
          ::close(value_);
        }

        value_ = value;
      }

    private:
      int value_;
    };

    struct Pipe
    {
      FileDescriptor read;
      FileDescriptor write;
    };

    [[nodiscard]] Result<Pipe> create_pipe()
    {
      std::array<int, 2> descriptors{};

      if (::pipe(descriptors.data()) != 0)
      {
        return Result<Pipe>::failure(
            Error{
                ErrorCode::io_error,
                "Failed to create process pipe: " +
                    std::string(std::strerror(errno))});
      }

      return Result<Pipe>::success(
          Pipe{
              .read = FileDescriptor{descriptors[0]},
              .write = FileDescriptor{descriptors[1]}});
    }

    [[nodiscard]] bool set_nonblocking(int descriptor)
    {
      const int flags = ::fcntl(descriptor, F_GETFL, 0);

      if (flags < 0)
      {
        return false;
      }

      return ::fcntl(
                 descriptor,
                 F_SETFL,
                 flags | O_NONBLOCK) == 0;
    }

    [[nodiscard]] Result<void> write_all(
        int descriptor,
        const std::string &input)
    {
      std::size_t offset = 0;

      while (offset < input.size())
      {
        const auto written = ::write(
            descriptor,
            input.data() + offset,
            input.size() - offset);

        if (written > 0)
        {
          offset += static_cast<std::size_t>(written);
          continue;
        }

        if (written < 0 && errno == EINTR)
        {
          continue;
        }

        return Result<void>::failure(
            Error{
                ErrorCode::io_error,
                "Failed to write to Python stdin: " +
                    std::string(std::strerror(errno))});
      }

      return Result<void>::success();
    }

    void read_available(
        int descriptor,
        std::string &output,
        bool &open)
    {
      std::array<char, 4096> buffer{};

      while (open)
      {
        const auto count = ::read(
            descriptor,
            buffer.data(),
            buffer.size());

        if (count > 0)
        {
          output.append(
              buffer.data(),
              static_cast<std::size_t>(count));
          continue;
        }

        if (count == 0)
        {
          open = false;
          return;
        }

        if (errno == EINTR)
        {
          continue;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
          return;
        }

        open = false;
      }
    }

    [[nodiscard]] std::vector<char *> build_arguments(
        const PythonProcessOptions &options,
        std::vector<std::string> &storage)
    {
      storage.clear();

      storage.push_back(options.executable.string());

      for (const auto &argument : options.arguments)
      {
        storage.push_back(argument);
      }

      storage.push_back(options.runtime_script.string());

      std::vector<char *> arguments;
      arguments.reserve(storage.size() + 1);

      for (auto &argument : storage)
      {
        arguments.push_back(argument.data());
      }

      arguments.push_back(nullptr);

      return arguments;
    }

    [[noreturn]] void execute_child(
        const PythonProcessOptions &options,
        Pipe &stdin_pipe,
        Pipe &stdout_pipe,
        Pipe &stderr_pipe)
    {
      ::dup2(stdin_pipe.read.get(), STDIN_FILENO);
      ::dup2(stdout_pipe.write.get(), STDOUT_FILENO);
      ::dup2(stderr_pipe.write.get(), STDERR_FILENO);

      stdin_pipe.read.reset();
      stdin_pipe.write.reset();
      stdout_pipe.read.reset();
      stdout_pipe.write.reset();
      stderr_pipe.read.reset();
      stderr_pipe.write.reset();

      if (!options.working_directory.empty())
      {
        const auto directory =
            options.working_directory.string();

        if (::chdir(directory.c_str()) != 0)
        {
          const std::string message =
              "Failed to change Python working directory: " +
              std::string(std::strerror(errno)) + "\n";

          ::write(
              STDERR_FILENO,
              message.data(),
              message.size());

          _exit(126);
        }
      }

      std::vector<std::string> argument_storage;
      auto arguments =
          build_arguments(options, argument_storage);

      const auto executable = options.executable.string();

      ::execvp(executable.c_str(), arguments.data());

      const std::string message =
          "Failed to launch Python executable '" +
          executable + "': " +
          std::string(std::strerror(errno)) + "\n";

      ::write(
          STDERR_FILENO,
          message.data(),
          message.size());

      _exit(errno == ENOENT ? 127 : 126);
    }

    [[nodiscard]] int decode_exit_status(int status) noexcept
    {
      if (WIFEXITED(status))
      {
        return WEXITSTATUS(status);
      }

      if (WIFSIGNALED(status))
      {
        return 128 + WTERMSIG(status);
      }

      return -1;
    }
  }

  Result<ProcessResult> run_python_process(
      const PythonProcessOptions &options,
      const std::string &input)
  {
    if (options.executable.empty())
    {
      return Result<ProcessResult>::failure(
          Error{
              ErrorCode::invalid_request,
              "Python executable cannot be empty"});
    }

    if (options.runtime_script.empty())
    {
      return Result<ProcessResult>::failure(
          Error{
              ErrorCode::invalid_request,
              "Python runtime script path cannot be empty"});
    }

    if (!std::filesystem::is_regular_file(
            options.runtime_script))
    {
      return Result<ProcessResult>::failure(
          Error{
              ErrorCode::io_error,
              "Python runtime script was not found: " +
                  options.runtime_script.string()});
    }

    auto stdin_result = create_pipe();

    if (!stdin_result)
    {
      return Result<ProcessResult>::failure(
          stdin_result.error());
    }

    auto stdout_result = create_pipe();

    if (!stdout_result)
    {
      return Result<ProcessResult>::failure(
          stdout_result.error());
    }

    auto stderr_result = create_pipe();

    if (!stderr_result)
    {
      return Result<ProcessResult>::failure(
          stderr_result.error());
    }

    auto stdin_pipe = stdin_result.take_value();
    auto stdout_pipe = stdout_result.take_value();
    auto stderr_pipe = stderr_result.take_value();

    const pid_t child = ::fork();

    if (child < 0)
    {
      return Result<ProcessResult>::failure(
          Error{
              ErrorCode::process_failed,
              "Failed to fork Python process: " +
                  std::string(std::strerror(errno))});
    }

    if (child == 0)
    {
      execute_child(
          options,
          stdin_pipe,
          stdout_pipe,
          stderr_pipe);
    }

    stdin_pipe.read.reset();
    stdout_pipe.write.reset();
    stderr_pipe.write.reset();

    const auto write_result =
        write_all(stdin_pipe.write.get(), input);

    stdin_pipe.write.reset();

    if (!write_result)
    {
      ::kill(child, SIGKILL);

      int ignored_status = 0;
      ::waitpid(child, &ignored_status, 0);

      return Result<ProcessResult>::failure(
          write_result.error());
    }

    set_nonblocking(stdout_pipe.read.get());
    set_nonblocking(stderr_pipe.read.get());

    ProcessResult result;

    bool stdout_open = true;
    bool stderr_open = true;
    bool child_running = true;

    int child_status = 0;

    const auto started_at =
        std::chrono::steady_clock::now();

    while (stdout_open || stderr_open || child_running)
    {
      if (
          options.timeout.count() > 0 &&
          std::chrono::steady_clock::now() - started_at >=
              options.timeout)
      {
        result.timed_out = true;

        ::kill(child, SIGKILL);
        ::waitpid(child, &child_status, 0);

        child_running = false;
      }

      std::array<pollfd, 2> descriptors{};
      nfds_t descriptor_count = 0;

      if (stdout_open)
      {
        descriptors[descriptor_count++] = pollfd{
            .fd = stdout_pipe.read.get(),
            .events = POLLIN | POLLHUP,
            .revents = 0};
      }

      if (stderr_open)
      {
        descriptors[descriptor_count++] = pollfd{
            .fd = stderr_pipe.read.get(),
            .events = POLLIN | POLLHUP,
            .revents = 0};
      }

      if (descriptor_count > 0)
      {
        const int poll_result =
            ::poll(
                descriptors.data(),
                descriptor_count,
                20);

        if (poll_result < 0 && errno != EINTR)
        {
          if (child_running)
          {
            ::kill(child, SIGKILL);
            ::waitpid(child, &child_status, 0);
          }

          return Result<ProcessResult>::failure(
              Error{
                  ErrorCode::io_error,
                  "Failed while reading Python output: " +
                      std::string(std::strerror(errno))});
        }

        read_available(
            stdout_pipe.read.get(),
            result.stdout_text,
            stdout_open);

        read_available(
            stderr_pipe.read.get(),
            result.stderr_text,
            stderr_open);
      }

      if (child_running)
      {
        const pid_t wait_result =
            ::waitpid(
                child,
                &child_status,
                WNOHANG);

        if (wait_result == child)
        {
          child_running = false;
        }
        else if (wait_result < 0 && errno != EINTR)
        {
          return Result<ProcessResult>::failure(
              Error{
                  ErrorCode::process_failed,
                  "Failed to wait for Python process: " +
                      std::string(std::strerror(errno))});
        }
      }

      if (
          !child_running &&
          !stdout_open &&
          !stderr_open)
      {
        break;
      }

      if (descriptor_count == 0 && child_running)
      {
        std::this_thread::sleep_for(
            std::chrono::milliseconds{5});
      }
    }

    read_available(
        stdout_pipe.read.get(),
        result.stdout_text,
        stdout_open);

    read_available(
        stderr_pipe.read.get(),
        result.stderr_text,
        stderr_open);

    stdout_pipe.read.reset();
    stderr_pipe.read.reset();

    result.exit_code = decode_exit_status(child_status);

    if (result.timed_out)
    {
      return Result<ProcessResult>::failure(
          Error{
              ErrorCode::timeout,
              "Python execution exceeded the configured timeout"});
    }

    if (result.exit_code == 127)
    {
      return Result<ProcessResult>::failure(
          Error{
              ErrorCode::python_not_found,
              result.stderr_text.empty()
                  ? "Python executable was not found"
                  : result.stderr_text});
    }

    return Result<ProcessResult>::success(
        std::move(result));
  }
}

#endif
