#pragma once

#include <functional>
#include <memory>
#include <vector>

#include "library/Database.h"
#include "library/Models.h"

namespace library {

/// Single entry point for all library business rules.
/// Both the Web server and the Qt app talk only to this class, so the rules
/// (validation, copy accounting, loan limits) live in exactly one place.
///
/// Not thread-safe: callers serialise access (the web server holds a mutex).
class LibraryService {
public:
    using Clock = std::function<Timestamp()>;

    static constexpr int kMaxActiveLoansPerStudent = 5;
    static constexpr int kDefaultLoanDays = 30;

    /// @param clock Injectable time source so overdue logic is testable.
    explicit LibraryService(std::shared_ptr<Database> db, Clock clock = systemClock);

    // ---- Books
    [[nodiscard]] std::vector<Book> listBooks(const BookQuery& query = {}) const;
    [[nodiscard]] Book getBook(std::int64_t id) const;
    Book addBook(const Book& book);
    /// Updates descriptive fields and total copies; available copies shift by the same delta.
    Book updateBook(const Book& book);
    void removeBook(std::int64_t id);

    // ---- Students
    [[nodiscard]] std::vector<Student> listStudents(const std::string& keyword = {}) const;
    [[nodiscard]] Student getStudent(std::int64_t id) const;
    Student addStudent(const Student& student);
    Student updateStudent(const Student& student);
    void removeStudent(std::int64_t id);

    // ---- Loans
    [[nodiscard]] std::vector<Loan> listLoans(const LoanQuery& query = {}) const;
    Loan borrowBook(std::int64_t studentId, std::int64_t bookId, int loanDays = kDefaultLoanDays);
    Loan returnBook(std::int64_t loanId);

    [[nodiscard]] LibraryStats stats() const;

    /// Number of loans started on each of the last `days` local days, oldest first
    /// (the last element is today). Days are bucketed by `utcOffsetSeconds`.
    [[nodiscard]] std::vector<int> borrowActivity(int days, int utcOffsetSeconds = 0) const;

    /// Most-borrowed titles of all time, most popular first.
    [[nodiscard]] std::vector<Book> popularBooks(int limit) const;

    /// Inserts sample data when the database is empty. Returns true if seeded.
    bool seedIfEmpty();

    [[nodiscard]] static Timestamp systemClock();

private:
    [[nodiscard]] Loan getLoan(std::int64_t id) const;
    static void validate(const Book& book);
    static void validate(const Student& student);

    std::shared_ptr<Database> db_;
    Clock clock_;
};

}  // namespace library
