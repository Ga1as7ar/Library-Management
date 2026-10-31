#include "JsonMapping.h"

#include "library/Errors.h"

namespace library {

using nlohmann::json;

namespace {

/// Reads an optional field, keeping the current value when absent.
template <typename T>
void readOptional(const json& j, const char* key, T& target) {
    if (auto it = j.find(key); it != j.end() && !it->is_null()) it->get_to(target);
}

}  // namespace

void to_json(json& j, const Book& book) {
    j = {
        {"id", book.id},
        {"isbn", book.isbn},
        {"title", book.title},
        {"author", book.author},
        {"publisher", book.publisher},
        {"priceCents", book.priceCents},
        {"totalCopies", book.totalCopies},
        {"availableCopies", book.availableCopies},
        {"category", toString(book.category)},
        {"subject", book.subject},
        {"course", book.course},
    };
}

void from_json(const json& j, Book& book) {
    readOptional(j, "id", book.id);
    readOptional(j, "isbn", book.isbn);
    readOptional(j, "title", book.title);
    readOptional(j, "author", book.author);
    readOptional(j, "publisher", book.publisher);
    readOptional(j, "priceCents", book.priceCents);
    readOptional(j, "totalCopies", book.totalCopies);
    readOptional(j, "subject", book.subject);
    readOptional(j, "course", book.course);

    std::string category = toString(book.category);
    readOptional(j, "category", category);
    const auto parsed = parseBookCategory(category);
    if (!parsed) throw LibraryError(ErrorCode::Validation, "未知的图书类别: " + category);
    book.category = *parsed;
}

void to_json(json& j, const Student& student) {
    j = {
        {"id", student.id},
        {"name", student.name},
        {"email", student.email},
        {"activeLoans", student.activeLoans},
    };
}

void from_json(const json& j, Student& student) {
    readOptional(j, "id", student.id);
    readOptional(j, "name", student.name);
    readOptional(j, "email", student.email);
}

void to_json(json& j, const Loan& loan) {
    j = {
        {"id", loan.id},
        {"bookId", loan.bookId},
        {"studentId", loan.studentId},
        {"bookTitle", loan.bookTitle},
        {"studentName", loan.studentName},
        {"borrowedAt", loan.borrowedAt},
        {"dueAt", loan.dueAt},
        {"returnedAt", loan.returnedAt ? json(*loan.returnedAt) : json(nullptr)},
    };
}

void to_json(json& j, const LibraryStats& stats) {
    j = {
        {"titles", stats.titles},
        {"totalCopies", stats.totalCopies},
        {"availableCopies", stats.availableCopies},
        {"students", stats.students},
        {"activeLoans", stats.activeLoans},
        {"overdueLoans", stats.overdueLoans},
    };
}

}  // namespace library
