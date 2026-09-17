#pragma once
#include "core/Error.h"

namespace dongle {
template <typename T>
struct Result {
    bool success{
        false
    };
    T value{};
    ErrorCode error{
        ErrorCode::InternalError
    };

    static Result ok(const T& value) {
        return {
            true, value, ErrorCode::None
        };
    }
    static Result fail(ErrorCode error) {
        Result result;
        result.error = error;
        return result;
    }
};

template <>
struct Result<void> {
    bool success{
        false
    };
    ErrorCode error{
        ErrorCode::InternalError
    };

    static Result ok() {
        return {
            true, ErrorCode::None
        };
    }
    static Result fail(ErrorCode error) {
        return {
            false, error
        };
    }
};
}
