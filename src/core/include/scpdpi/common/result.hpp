#pragma once

#include <stdexcept>
#include <string>
#include <optional>
#include <utility>
#include <variant>

namespace scpdpi {

struct Error {
    std::string message;
    int native_code{};
};

template <class T>
class Result {
public:
    static Result success(T value) { return Result(std::move(value)); }
    static Result failure(Error error) { return Result(std::move(error)); }
    [[nodiscard]] bool has_value() const noexcept { return std::holds_alternative<T>(data_); }
    explicit operator bool() const noexcept { return has_value(); }
    [[nodiscard]] const T& value() const {
        if (!has_value()) throw std::logic_error("Result contains an error");
        return std::get<T>(data_);
    }
    [[nodiscard]] T& value() {
        if (!has_value()) throw std::logic_error("Result contains an error");
        return std::get<T>(data_);
    }
    [[nodiscard]] const Error& error() const {
        if (has_value()) throw std::logic_error("Result contains a value");
        return std::get<Error>(data_);
    }
private:
    explicit Result(T value) : data_(std::move(value)) {}
    explicit Result(Error error) : data_(std::move(error)) {}
    std::variant<T, Error> data_;
};

template <>
class Result<void> {
public:
    static Result success() { return Result(); }
    static Result failure(Error error) { return Result(std::move(error)); }
    [[nodiscard]] bool has_value() const noexcept { return !error_.has_value(); }
    explicit operator bool() const noexcept { return has_value(); }
    [[nodiscard]] const Error& error() const {
        if (!error_) throw std::logic_error("Result contains no error");
        return *error_;
    }
private:
    Result() = default;
    explicit Result(Error error) : error_(std::move(error)) {}
    std::optional<Error> error_;
};

} // namespace scpdpi
