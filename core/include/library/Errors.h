#pragma once

#include <stdexcept>
#include <string>

namespace library {

/// Error categories, mapped 1:1 to HTTP statuses / dialog types by the front-ends.
enum class ErrorCode {
    Validation,    ///< Input is malformed (empty title, bad ISBN, ...).
    NotFound,      ///< Referenced entity does not exist.
    Conflict,      ///< Uniqueness violation (duplicate ISBN / student id).
    BusinessRule,  ///< Valid input that breaks a library rule (no copies left, ...).
    Database,      ///< Underlying storage failure.
};

class LibraryError : public std::runtime_error {
public:
    LibraryError(ErrorCode code, const std::string& message)
        : std::runtime_error(message), code_(code) {}

    [[nodiscard]] ErrorCode code() const noexcept { return code_; }

private:
    ErrorCode code_;
};

}  // namespace library
