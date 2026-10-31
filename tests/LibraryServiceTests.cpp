#include <doctest/doctest.h>

#include <clocale>

#include "library/Errors.h"
#include "library/LibraryService.h"

using namespace library;

namespace {

constexpr Timestamp kDay = 24 * 60 * 60;

/// Fresh in-memory library with a controllable clock.
struct Fixture {
    Timestamp now = 1'700'000'000;
    LibraryService service{std::make_shared<Database>(":memory:"), [this] { return now; }};

    Book makeBook(const std::string& isbn = "9787121155352", int copies = 2) {
        Book book;
        book.isbn = isbn;
        book.title = "C++ Primer";
        book.author = "Lippman";
        book.totalCopies = copies;
        return service.addBook(book);
    }

    Student makeStudent(std::int64_t id = 5120255017, const std::string& name = "周星可") {
        return service.addStudent({id, name, "", 0});
    }
};

ErrorCode errorOf(const auto& action) {
    try {
        action();
    } catch (const LibraryError& error) {
        return error.code();
    }
    FAIL("expected LibraryError");
    return ErrorCode::Database;
}

}  // namespace

TEST_CASE_FIXTURE(Fixture, "books: add, normalise and query") {
    Book input;
    input.isbn = "978-7-121-15535-2";
    input.title = "  C++ Primer  ";
    input.totalCopies = 3;
    input.subject = "ignored for general books";
    const Book book = service.addBook(input);

    CHECK(book.isbn == "9787121155352");
    CHECK(book.title == "C++ Primer");
    CHECK(book.availableCopies == 3);
    CHECK(book.subject.empty());

    CHECK(service.listBooks({.keyword = "primer"}).size() == 1);
    CHECK(service.listBooks({.keyword = "nothing"}).empty());
    CHECK(service.listBooks({.keyword = "%"}).empty());  // wildcards are matched literally
    CHECK(service.listBooks({.category = BookCategory::Textbook}).empty());
}

TEST_CASE_FIXTURE(Fixture, "books: validation and uniqueness") {
    Book bad;
    bad.title = "X";
    bad.isbn = "9787121155353";  // wrong check digit
    CHECK(errorOf([&] { service.addBook(bad); }) == ErrorCode::Validation);

    bad.isbn = "0-306-40615-2";  // valid ISBN-10
    bad.title = "";
    CHECK(errorOf([&] { service.addBook(bad); }) == ErrorCode::Validation);

    makeBook();
    CHECK(errorOf([&] { makeBook(); }) == ErrorCode::Conflict);
}

TEST_CASE_FIXTURE(Fixture, "borrow and return keep copy counts consistent") {
    const Book book = makeBook("9787121155352", 1);
    makeStudent(1, "甲");
    makeStudent(2, "乙");

    const Loan loan = service.borrowBook(1, book.id, 30);
    CHECK(loan.isActive());
    CHECK(loan.dueAt == now + 30 * kDay);
    CHECK(service.getBook(book.id).availableCopies == 0);
    CHECK(service.getStudent(1).activeLoans == 1);

    // The original program printed an error but still "borrowed"; now it is rejected.
    CHECK(errorOf([&] { service.borrowBook(2, book.id); }) == ErrorCode::BusinessRule);

    const Loan returned = service.returnBook(loan.id);
    CHECK_FALSE(returned.isActive());
    CHECK(service.getBook(book.id).availableCopies == 1);
    CHECK(errorOf([&] { service.returnBook(loan.id); }) == ErrorCode::BusinessRule);
}

TEST_CASE_FIXTURE(Fixture, "borrow rules: duplicates, limit and overdue") {
    makeStudent(1, "甲");
    const Book first = makeBook("9787121155352", 5);
    service.borrowBook(1, first.id);
    CHECK(errorOf([&] { service.borrowBook(1, first.id); }) == ErrorCode::BusinessRule);

    const char* isbns[] = {"9787111544937", "9787040396638", "9787544253994", "9787020002207", "9787508647357"};
    for (int i = 0; i < 4; ++i) service.borrowBook(1, makeBook(isbns[i]).id);
    CHECK(service.getStudent(1).activeLoans == LibraryService::kMaxActiveLoansPerStudent);
    CHECK(errorOf([&] { service.borrowBook(1, makeBook(isbns[4]).id); }) == ErrorCode::BusinessRule);

    makeStudent(2, "乙");
    service.borrowBook(2, first.id, 7);
    now += 8 * kDay;
    CHECK(service.stats().overdueLoans == 1);  // only the 7-day loan has expired
    CHECK(service.listLoans({.studentId = 2, .filter = LoanFilter::Overdue}).size() == 1);
    // A student with an overdue loan cannot borrow anything else (isbns[4] has copies left).
    const auto spareBookId = service.listBooks({.keyword = isbns[4]}).front().id;
    CHECK(errorOf([&] { service.borrowBook(2, spareBookId); }) == ErrorCode::BusinessRule);
}

TEST_CASE_FIXTURE(Fixture, "deleting requires no active loans") {
    const Book book = makeBook();
    makeStudent();
    const Loan loan = service.borrowBook(5120255017, book.id);

    CHECK(errorOf([&] { service.removeBook(book.id); }) == ErrorCode::BusinessRule);
    CHECK(errorOf([&] { service.removeStudent(5120255017); }) == ErrorCode::BusinessRule);

    service.returnBook(loan.id);
    service.removeStudent(5120255017);
    service.removeBook(book.id);
    CHECK(service.stats().titles == 0);
    CHECK(errorOf([&] { (void)service.getBook(book.id); }) == ErrorCode::NotFound);
}

TEST_CASE_FIXTURE(Fixture, "update book cannot drop below borrowed copies") {
    Book book = makeBook("9787121155352", 3);
    makeStudent();
    service.borrowBook(5120255017, book.id);

    book.totalCopies = 0;
    CHECK(errorOf([&] { service.updateBook(book); }) == ErrorCode::BusinessRule);

    book.totalCopies = 5;
    book.title = "C++ Primer 5th";
    const Book updated = service.updateBook(book);
    CHECK(updated.availableCopies == 4);
    CHECK(updated.title == "C++ Primer 5th");
}

TEST_CASE_FIXTURE(Fixture, "transactions roll back on failure") {
    makeStudent();
    // Borrowing a missing book throws mid-transaction; nothing must be written.
    CHECK(errorOf([&] { service.borrowBook(5120255017, 999); }) == ErrorCode::NotFound);
    CHECK(service.listLoans().empty());
    // And the connection is still usable (no dangling transaction).
    CHECK(service.seedIfEmpty() == false);
}

TEST_CASE_FIXTURE(Fixture, "trimming never splits UTF-8 characters, whatever the locale") {
    // 悠 = E6 82 A0; 0xA0 counts as whitespace for isspace() under some UTF-8 locales.
    std::setlocale(LC_ALL, "en_US.UTF-8");
    const Student student = service.addStudent({42, "  王悠悠  ", "", 0});
    std::setlocale(LC_ALL, "C");
    CHECK(student.name == "王悠悠");
}

TEST_CASE("seed populates an empty database exactly once") {
    LibraryService service(std::make_shared<Database>(":memory:"));
    CHECK(service.seedIfEmpty());
    CHECK_FALSE(service.seedIfEmpty());
    const auto stats = service.stats();
    CHECK(stats.titles == 7);
    CHECK(stats.students == 5);
    CHECK(stats.activeLoans == 6);
    CHECK(stats.overdueLoans == 1);
    CHECK(stats.availableCopies == stats.totalCopies - stats.activeLoans);  // copy counts stay consistent
}

TEST_CASE_FIXTURE(Fixture, "activity buckets loans by local day and popular books rank by loans") {
    const Book a = makeBook("9787121155352", 5);
    const Book b = makeBook("9787111544937", 5);
    makeStudent(1, "甲");
    makeStudent(2, "乙");

    now -= 2 * kDay;
    service.returnBook(service.borrowBook(1, b.id).id);
    now += 2 * kDay;
    service.borrowBook(1, a.id);
    service.borrowBook(2, a.id);

    const auto activity = service.borrowActivity(7);
    REQUIRE(activity.size() == 7);
    CHECK(activity[6] == 2);  // today
    CHECK(activity[4] == 1);  // two days ago
    CHECK(activity[0] == 0);

    const auto popular = service.popularBooks(5);
    REQUIRE(popular.size() == 2);
    CHECK(popular.front().id == a.id);
}
