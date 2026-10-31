#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace library {

/// Seconds since the Unix epoch (UTC).
using Timestamp = std::int64_t;

enum class BookCategory { General, Textbook };

[[nodiscard]] std::string toString(BookCategory category);
[[nodiscard]] std::optional<BookCategory> parseBookCategory(std::string_view text);

/// A catalogued title together with its copy counts.
/// Textbooks are a category rather than a subclass: subject/course are simply
/// empty for general books, which keeps storage and serialisation flat.
struct Book {
    std::int64_t id = 0;
    std::string isbn;
    std::string title;
    std::string author;
    std::string publisher;
    std::int64_t priceCents = 0;  ///< Money as integer cents — never floating point.
    int totalCopies = 1;
    int availableCopies = 1;
    BookCategory category = BookCategory::General;
    std::string subject;  ///< Textbook only.
    std::string course;   ///< Textbook only.

    [[nodiscard]] bool canBorrow() const { return availableCopies > 0; }
    [[nodiscard]] int borrowedCopies() const { return totalCopies - availableCopies; }
};

struct Student {
    std::int64_t id = 0;  ///< Student number, chosen by the school (64-bit: real ids exceed INT_MAX).
    std::string name;
    std::string email;
    int activeLoans = 0;  ///< Derived, read-only.
};

struct Loan {
    std::int64_t id = 0;
    std::int64_t bookId = 0;
    std::int64_t studentId = 0;
    std::string bookTitle;    ///< Denormalised for display.
    std::string studentName;  ///< Denormalised for display.
    Timestamp borrowedAt = 0;
    Timestamp dueAt = 0;
    std::optional<Timestamp> returnedAt;

    [[nodiscard]] bool isActive() const { return !returnedAt.has_value(); }
    [[nodiscard]] bool isOverdue(Timestamp now) const { return isActive() && now > dueAt; }
};

struct BookQuery {
    std::string keyword;  ///< Matches title, author, ISBN or publisher.
    std::optional<BookCategory> category;
    bool onlyAvailable = false;
};

enum class LoanFilter { All, Active, Overdue, Returned };

struct LoanQuery {
    std::optional<std::int64_t> studentId;
    std::optional<std::int64_t> bookId;
    LoanFilter filter = LoanFilter::All;
};

struct LibraryStats {
    int titles = 0;
    int totalCopies = 0;
    int availableCopies = 0;
    int students = 0;
    int activeLoans = 0;
    int overdueLoans = 0;
};

}  // namespace library
