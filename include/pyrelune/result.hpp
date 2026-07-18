/**
 * @file result.hpp
 * @brief Provides explicit success and failure results for Pyrelune.
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

#include <pyrelune/error.hpp>

#include <optional>
#include <stdexcept>
#include <utility>
#include <variant>

namespace pyrelune
{
  /**
   * @brief Stores either a successful value or an Error.
   *
   * @tparam T Successful value type.
   */
  template <typename T>
  class Result
  {
  public:
    /**
     * @brief Creates a successful result.
     */
    [[nodiscard]] static Result success(T value)
    {
      return Result(std::move(value));
    }

    /**
     * @brief Creates a failed result.
     */
    [[nodiscard]] static Result failure(Error error)
    {
      return Result(std::move(error));
    }

    /**
     * @brief Returns true when the result contains a value.
     */
    [[nodiscard]] bool has_value() const noexcept
    {
      return std::holds_alternative<T>(storage_);
    }

    /**
     * @brief Returns true when the result contains an error.
     */
    [[nodiscard]] bool has_error() const noexcept
    {
      return std::holds_alternative<Error>(storage_);
    }

    /**
     * @brief Returns true when the operation succeeded.
     */
    [[nodiscard]] explicit operator bool() const noexcept
    {
      return has_value();
    }

    /**
     * @brief Returns the successful value.
     *
     * @throws std::logic_error If the result contains an error.
     */
    [[nodiscard]] T &value()
    {
      if (!has_value())
      {
        throw std::logic_error(
            "Attempted to access the value of a failed Result");
      }

      return std::get<T>(storage_);
    }

    /**
     * @brief Returns the successful value.
     *
     * @throws std::logic_error If the result contains an error.
     */
    [[nodiscard]] const T &value() const
    {
      if (!has_value())
      {
        throw std::logic_error(
            "Attempted to access the value of a failed Result");
      }

      return std::get<T>(storage_);
    }

    /**
     * @brief Moves the successful value out of the result.
     *
     * @throws std::logic_error If the result contains an error.
     */
    [[nodiscard]] T take_value()
    {
      if (!has_value())
      {
        throw std::logic_error(
            "Attempted to move the value of a failed Result");
      }

      return std::move(std::get<T>(storage_));
    }

    /**
     * @brief Returns the stored error.
     *
     * @throws std::logic_error If the result contains a value.
     */
    [[nodiscard]] Error &error()
    {
      if (!has_error())
      {
        throw std::logic_error(
            "Attempted to access the error of a successful Result");
      }

      return std::get<Error>(storage_);
    }

    /**
     * @brief Returns the stored error.
     *
     * @throws std::logic_error If the result contains a value.
     */
    [[nodiscard]] const Error &error() const
    {
      if (!has_error())
      {
        throw std::logic_error(
            "Attempted to access the error of a successful Result");
      }

      return std::get<Error>(storage_);
    }

  private:
    explicit Result(T value)
        : storage_(std::move(value))
    {
    }

    explicit Result(Error error)
        : storage_(std::move(error))
    {
    }

    std::variant<T, Error> storage_;
  };

  /**
   * @brief Specialization for operations that do not return a value.
   */
  template <>
  class Result<void>
  {
  public:
    /**
     * @brief Creates a successful result.
     */
    [[nodiscard]] static Result success()
    {
      return Result(std::nullopt);
    }

    /**
     * @brief Creates a failed result.
     */
    [[nodiscard]] static Result failure(Error error)
    {
      return Result(std::move(error));
    }

    /**
     * @brief Returns true when the operation succeeded.
     */
    [[nodiscard]] bool has_value() const noexcept
    {
      return !error_.has_value();
    }

    /**
     * @brief Returns true when the operation failed.
     */
    [[nodiscard]] bool has_error() const noexcept
    {
      return error_.has_value();
    }

    /**
     * @brief Returns true when the operation succeeded.
     */
    [[nodiscard]] explicit operator bool() const noexcept
    {
      return has_value();
    }

    /**
     * @brief Returns the stored error.
     *
     * @throws std::logic_error If the operation succeeded.
     */
    [[nodiscard]] const Error &error() const
    {
      if (!error_)
      {
        throw std::logic_error(
            "Attempted to access the error of a successful Result");
      }

      return *error_;
    }

  private:
    explicit Result(std::optional<Error> error)
        : error_(std::move(error))
    {
    }

    explicit Result(Error error)
        : error_(std::move(error))
    {
    }

    std::optional<Error> error_;
  };
}
