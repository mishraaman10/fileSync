// Result.hpp
#pragma once
#include <string>

template<typename T>
struct Result {
    bool success;
    std::string message;
    T data;

    static Result<T> Ok(const T& data) {
        return {true, "", data};
    }

    static Result<T> Error(const std::string& msg) {
        return {false, msg, T{}};
    }
};

// Specialization for void
template<>
struct Result<void> {
    bool success;
    std::string message;

    static Result<void> Ok() {
        return {true, ""};
    }

    static Result<void> Error(const std::string& msg) {
        return {false, msg};
    }
};