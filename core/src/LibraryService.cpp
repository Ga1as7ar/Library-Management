#include "library/LibraryService.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <utility>

#include "library/Errors.h"

namespace library {

namespace {

constexpr Timestamp kSecondsPerDay = 24 * 60 * 60;

constexpr const char* kBookColumns =
    "id, isbn, title, author, publisher, price_cents, total_copies, available_copies, "
    "category, subject, course";

constexpr const char* kLoanSelect = R"sql(
    SELECT l.id, l.book_id, l.student_id, b.title, s.name, l.borrowed_at, l.due_at, l.returned_at
    FROM loans l
    JOIN books b    ON b.id = l.book_id
    JOIN students s ON s.id = l.student_id
)sql";

[[noreturn]] void fail(ErrorCode code, const std::string& message) { throw LibraryError(code, message); }

/// Trims ASCII whitespace only. std::isspace is locale-dependent: under a UTF-8
/// locale on macOS it treats byte 0xA0 as space and would cut multi-byte
/// characters such as "悠" (E6 82 A0) in half.
std::string trim(std::string_view text) {
    const auto isSpace = [](char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v'; };
    auto begin = std::find_if_not(text.begin(), text.end(), isSpace);
    auto end = std::find_if_not(text.rbegin(), std::string_view::reverse_iterator(begin), isSpace).base();
    return {begin, end};
}

/// Escapes LIKE wildcards so user input is matched literally.
std::string likePattern(std::string_view keyword) {
    std::string pattern = "%";
    for (char c : keyword) {
        if (c == '%' || c == '_' || c == '\\') pattern += '\\';
        pattern += c;
    }
    return pattern + "%";
}

/// Accepts ISBN-10 / ISBN-13 with optional hyphens or spaces; returns digits only.
std::string normaliseIsbn(std::string_view raw) {
    std::string digits;
    for (char c : raw) {
        if (c == '-' || c == ' ') continue;
        digits += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return digits;
}

bool isValidIsbn(const std::string& isbn) {
    if (isbn.size() == 10) {
        int sum = 0;
        for (std::size_t i = 0; i < 10; ++i) {
            const char c = isbn[i];
            int value;
            if (std::isdigit(static_cast<unsigned char>(c))) {
                value = c - '0';
            } else if (c == 'X' && i == 9) {
                value = 10;
            } else {
                return false;
            }
            sum += value * static_cast<int>(10 - i);
        }
        return sum % 11 == 0;
    }
    if (isbn.size() == 13) {
        int sum = 0;
        for (std::size_t i = 0; i < 13; ++i) {
            if (!std::isdigit(static_cast<unsigned char>(isbn[i]))) return false;
            sum += (isbn[i] - '0') * (i % 2 == 0 ? 1 : 3);
        }
        return sum % 10 == 0;
    }
    return false;
}

Book readBook(const Statement& row) {
    Book book;
    book.id = row.getInt(0);
    book.isbn = row.getText(1);
    book.title = row.getText(2);
    book.author = row.getText(3);
    book.publisher = row.getText(4);
    book.priceCents = row.getInt(5);
    book.totalCopies = static_cast<int>(row.getInt(6));
    book.availableCopies = static_cast<int>(row.getInt(7));
    book.category = parseBookCategory(row.getText(8)).value_or(BookCategory::General);
    book.subject = row.getText(9);
    book.course = row.getText(10);
    return book;
}

Loan readLoan(const Statement& row) {
    Loan loan;
    loan.id = row.getInt(0);
    loan.bookId = row.getInt(1);
    loan.studentId = row.getInt(2);
    loan.bookTitle = row.getText(3);
    loan.studentName = row.getText(4);
    loan.borrowedAt = row.getInt(5);
    loan.dueAt = row.getInt(6);
    loan.returnedAt = row.getOptionalInt(7);
    return loan;
}

Book normalised(Book book) {
    book.isbn = normaliseIsbn(book.isbn);
    book.title = trim(book.title);
    book.author = trim(book.author);
    book.publisher = trim(book.publisher);
    if (book.category == BookCategory::General) {
        book.subject.clear();
        book.course.clear();
    } else {
        book.subject = trim(book.subject);
        book.course = trim(book.course);
    }
    return book;
}

Student normalised(Student student) {
    student.name = trim(student.name);
    student.email = trim(student.email);
    return student;
}

}  // namespace

LibraryService::LibraryService(std::shared_ptr<Database> db, Clock clock)
    : db_(std::move(db)), clock_(std::move(clock)) {
    if (!db_) fail(ErrorCode::Database, "数据库未初始化");
}

Timestamp LibraryService::systemClock() {
    using namespace std::chrono;
    return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

// ======================================================================== Books

void LibraryService::validate(const Book& book) {
    if (book.title.empty()) fail(ErrorCode::Validation, "书名不能为空");
    if (book.title.size() > 200) fail(ErrorCode::Validation, "书名过长（最多 200 字节）");
    if (!isValidIsbn(book.isbn)) fail(ErrorCode::Validation, "ISBN 无效：需为 10 位或 13 位且校验位正确");
    if (book.priceCents < 0) fail(ErrorCode::Validation, "价格不能为负数");
    if (book.totalCopies < 0 || book.totalCopies > 9999) fail(ErrorCode::Validation, "馆藏数量需在 0–9999 之间");
}

std::vector<Book> LibraryService::listBooks(const BookQuery& query) const {
    std::string sql = std::string("SELECT ") + kBookColumns +
                      " FROM books WHERE (?1 = '' OR title LIKE ?2 ESCAPE '\\' OR author LIKE ?2 ESCAPE '\\'"
                      " OR isbn LIKE ?2 ESCAPE '\\' OR publisher LIKE ?2 ESCAPE '\\')"
                      " AND (?3 IS NULL OR category = ?3)"
                      " AND (?4 = 0 OR available_copies > 0)"
                      " ORDER BY title COLLATE NOCASE";
    const std::string keyword = trim(query.keyword);
    auto stmt = db_->prepare(sql);
    stmt.bindAll(keyword, likePattern(keyword));
    query.category ? stmt.bind(3, toString(*query.category)) : stmt.bindNull(3);
    stmt.bind(4, query.onlyAvailable ? 1 : 0);

    std::vector<Book> books;
    while (stmt.step()) books.push_back(readBook(stmt));
    return books;
}

Book LibraryService::getBook(std::int64_t id) const {
    auto stmt = db_->prepare(std::string("SELECT ") + kBookColumns + " FROM books WHERE id = ?");
    stmt.bind(1, id);
    if (!stmt.step()) fail(ErrorCode::NotFound, "图书不存在（编号 " + std::to_string(id) + "）");
    return readBook(stmt);
}

Book LibraryService::addBook(const Book& input) {
    const Book book = normalised(input);
    validate(book);
    try {
        db_->prepare(R"sql(
            INSERT INTO books (isbn, title, author, publisher, price_cents,
                               total_copies, available_copies, category, subject, course)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?))sql")
            .bindAll(book.isbn, book.title, book.author, book.publisher, book.priceCents, book.totalCopies,
                     book.totalCopies, toString(book.category), book.subject, book.course)
            .run();
    } catch (const LibraryError& error) {
        if (error.code() == ErrorCode::Conflict) fail(ErrorCode::Conflict, "ISBN " + book.isbn + " 已存在");
        throw;
    }
    return getBook(db_->lastInsertId());
}

Book LibraryService::updateBook(const Book& input) {
    const Book book = normalised(input);
    validate(book);

    Transaction tx(*db_);
    const Book current = getBook(book.id);
    const int borrowed = current.borrowedCopies();
    if (book.totalCopies < borrowed) {
        fail(ErrorCode::BusinessRule,
             "馆藏数量不能少于已借出数量（当前借出 " + std::to_string(borrowed) + " 本）");
    }
    try {
        db_->prepare(R"sql(
            UPDATE books SET isbn = ?, title = ?, author = ?, publisher = ?, price_cents = ?,
                             total_copies = ?, available_copies = ?, category = ?, subject = ?, course = ?
            WHERE id = ?)sql")
            .bindAll(book.isbn, book.title, book.author, book.publisher, book.priceCents, book.totalCopies,
                     book.totalCopies - borrowed, toString(book.category), book.subject, book.course, book.id)
            .run();
    } catch (const LibraryError& error) {
        if (error.code() == ErrorCode::Conflict) fail(ErrorCode::Conflict, "ISBN " + book.isbn + " 已被其他图书使用");
        throw;
    }
    tx.commit();
    return getBook(book.id);
}

void LibraryService::removeBook(std::int64_t id) {
    Transaction tx(*db_);
    const Book book = getBook(id);
    if (book.borrowedCopies() > 0) fail(ErrorCode::BusinessRule, "《" + book.title + "》仍有未归还的借阅，无法删除");
    // Returned loans are history for this title only; drop them with the book.
    db_->prepare("DELETE FROM loans WHERE book_id = ?").bind(1, id).run();
    db_->prepare("DELETE FROM books WHERE id = ?").bind(1, id).run();
    tx.commit();
}

// ===================================================================== Students

void LibraryService::validate(const Student& student) {
    if (student.id <= 0) fail(ErrorCode::Validation, "学号必须是正整数");
    if (student.name.empty()) fail(ErrorCode::Validation, "姓名不能为空");
    if (student.name.size() > 100) fail(ErrorCode::Validation, "姓名过长");
    if (!student.email.empty()) {
        const auto at = student.email.find('@');
        if (at == std::string::npos || at == 0 || student.email.find('.', at) == std::string::npos) {
            fail(ErrorCode::Validation, "邮箱格式不正确");
        }
    }
}

std::vector<Student> LibraryService::listStudents(const std::string& rawKeyword) const {
    const std::string keyword = trim(rawKeyword);
    auto stmt = db_->prepare(R"sql(
        SELECT s.id, s.name, s.email,
               (SELECT COUNT(*) FROM loans l WHERE l.student_id = s.id AND l.returned_at IS NULL)
        FROM students s
        WHERE ?1 = '' OR s.name LIKE ?2 ESCAPE '\' OR CAST(s.id AS TEXT) LIKE ?2 ESCAPE '\'
        ORDER BY s.id)sql");
    stmt.bindAll(keyword, likePattern(keyword));

    std::vector<Student> students;
    while (stmt.step()) {
        students.push_back({stmt.getInt(0), stmt.getText(1), stmt.getText(2), static_cast<int>(stmt.getInt(3))});
    }
    return students;
}

Student LibraryService::getStudent(std::int64_t id) const {
    auto stmt = db_->prepare(R"sql(
        SELECT s.id, s.name, s.email,
               (SELECT COUNT(*) FROM loans l WHERE l.student_id = s.id AND l.returned_at IS NULL)
        FROM students s WHERE s.id = ?)sql");
    stmt.bind(1, id);
    if (!stmt.step()) fail(ErrorCode::NotFound, "学生不存在（学号 " + std::to_string(id) + "）");
    return {stmt.getInt(0), stmt.getText(1), stmt.getText(2), static_cast<int>(stmt.getInt(3))};
}

Student LibraryService::addStudent(const Student& input) {
    const Student student = normalised(input);
    validate(student);
    try {
        db_->prepare("INSERT INTO students (id, name, email) VALUES (?, ?, ?)")
            .bindAll(student.id, student.name, student.email)
            .run();
    } catch (const LibraryError& error) {
        if (error.code() == ErrorCode::Conflict) {
            fail(ErrorCode::Conflict, "学号 " + std::to_string(student.id) + " 已存在");
        }
        throw;
    }
    return getStudent(student.id);
}

Student LibraryService::updateStudent(const Student& input) {
    const Student student = normalised(input);
    validate(student);
    db_->prepare("UPDATE students SET name = ?, email = ? WHERE id = ?")
        .bindAll(student.name, student.email, student.id)
        .run();
    if (db_->changes() == 0) fail(ErrorCode::NotFound, "学生不存在（学号 " + std::to_string(student.id) + "）");
    return getStudent(student.id);
}

void LibraryService::removeStudent(std::int64_t id) {
    Transaction tx(*db_);
    const Student student = getStudent(id);
    if (student.activeLoans > 0) fail(ErrorCode::BusinessRule, student.name + " 还有未归还的图书，无法删除");
    db_->prepare("DELETE FROM loans WHERE student_id = ?").bind(1, id).run();
    db_->prepare("DELETE FROM students WHERE id = ?").bind(1, id).run();
    tx.commit();
}

// ======================================================================== Loans

std::vector<Loan> LibraryService::listLoans(const LoanQuery& query) const {
    std::string sql = std::string(kLoanSelect) +
                      " WHERE (?1 IS NULL OR l.student_id = ?1) AND (?2 IS NULL OR l.book_id = ?2)";
    switch (query.filter) {
        case LoanFilter::Active: sql += " AND l.returned_at IS NULL"; break;
        case LoanFilter::Overdue: sql += " AND l.returned_at IS NULL AND l.due_at < ?3"; break;
        case LoanFilter::Returned: sql += " AND l.returned_at IS NOT NULL"; break;
        case LoanFilter::All: break;
    }
    // Active loans first (soonest due on top), then history newest first.
    sql += " ORDER BY (l.returned_at IS NOT NULL), CASE WHEN l.returned_at IS NULL THEN l.due_at END,"
           " l.returned_at DESC";

    auto stmt = db_->prepare(sql);
    stmt.bind(1, query.studentId).bind(2, query.bookId);
    if (query.filter == LoanFilter::Overdue) stmt.bind(3, clock_());

    std::vector<Loan> loans;
    while (stmt.step()) loans.push_back(readLoan(stmt));
    return loans;
}

Loan LibraryService::getLoan(std::int64_t id) const {
    auto stmt = db_->prepare(std::string(kLoanSelect) + " WHERE l.id = ?");
    stmt.bind(1, id);
    if (!stmt.step()) fail(ErrorCode::NotFound, "借阅记录不存在（编号 " + std::to_string(id) + "）");
    return readLoan(stmt);
}

Loan LibraryService::borrowBook(std::int64_t studentId, std::int64_t bookId, int loanDays) {
    if (loanDays < 1 || loanDays > 180) fail(ErrorCode::Validation, "借阅天数需在 1–180 天之间");

    Transaction tx(*db_);
    const Student student = getStudent(studentId);
    const Book book = getBook(bookId);
    const Timestamp now = clock_();

    if (!book.canBorrow()) fail(ErrorCode::BusinessRule, "《" + book.title + "》已全部借出");
    if (student.activeLoans >= kMaxActiveLoansPerStudent) {
        fail(ErrorCode::BusinessRule,
             student.name + " 已借 " + std::to_string(student.activeLoans) + " 本，达到上限 " +
                 std::to_string(kMaxActiveLoansPerStudent) + " 本");
    }
    {
        auto overdue = db_->prepare(
            "SELECT COUNT(*) FROM loans WHERE student_id = ? AND returned_at IS NULL AND due_at < ?");
        overdue.bindAll(studentId, now).step();
        if (overdue.getInt(0) > 0) fail(ErrorCode::BusinessRule, student.name + " 有逾期未还的图书，请先归还");
    }
    {
        auto duplicate =
            db_->prepare("SELECT COUNT(*) FROM loans WHERE student_id = ? AND book_id = ? AND returned_at IS NULL");
        duplicate.bindAll(studentId, bookId).step();
        if (duplicate.getInt(0) > 0) fail(ErrorCode::BusinessRule, student.name + " 已借阅《" + book.title + "》");
    }

    db_->prepare("UPDATE books SET available_copies = available_copies - 1 WHERE id = ?").bind(1, bookId).run();
    db_->prepare("INSERT INTO loans (book_id, student_id, borrowed_at, due_at) VALUES (?, ?, ?, ?)")
        .bindAll(bookId, studentId, now, now + loanDays * kSecondsPerDay)
        .run();
    const auto loanId = db_->lastInsertId();
    tx.commit();
    return getLoan(loanId);
}

Loan LibraryService::returnBook(std::int64_t loanId) {
    Transaction tx(*db_);
    const Loan loan = getLoan(loanId);
    if (!loan.isActive()) fail(ErrorCode::BusinessRule, "该借阅记录已归还");

    db_->prepare("UPDATE loans SET returned_at = ? WHERE id = ?").bindAll(clock_(), loanId).run();
    db_->prepare("UPDATE books SET available_copies = available_copies + 1 WHERE id = ?")
        .bind(1, loan.bookId)
        .run();
    tx.commit();
    return getLoan(loanId);
}

// ======================================================================== Stats

LibraryStats LibraryService::stats() const {
    auto stmt = db_->prepare(R"sql(
        SELECT (SELECT COUNT(*) FROM books),
               (SELECT COALESCE(SUM(total_copies), 0) FROM books),
               (SELECT COALESCE(SUM(available_copies), 0) FROM books),
               (SELECT COUNT(*) FROM students),
               (SELECT COUNT(*) FROM loans WHERE returned_at IS NULL),
               (SELECT COUNT(*) FROM loans WHERE returned_at IS NULL AND due_at < ?))sql");
    stmt.bind(1, clock_()).step();
    return {static_cast<int>(stmt.getInt(0)), static_cast<int>(stmt.getInt(1)), static_cast<int>(stmt.getInt(2)),
            static_cast<int>(stmt.getInt(3)), static_cast<int>(stmt.getInt(4)), static_cast<int>(stmt.getInt(5))};
}

std::vector<int> LibraryService::borrowActivity(int days, int utcOffsetSeconds) const {
    if (days < 1 || days > 366) fail(ErrorCode::Validation, "统计天数需在 1–366 之间");
    // Day index relative to today in local time: 0 = today, 1 = yesterday, ...
    auto stmt = db_->prepare(R"sql(
        SELECT (?1 + ?2) / 86400 - (borrowed_at + ?2) / 86400 AS ago, COUNT(*)
        FROM loans
        WHERE borrowed_at > ?1 - ?3 * 86400
        GROUP BY ago)sql");
    stmt.bindAll(clock_(), utcOffsetSeconds, days);

    std::vector<int> counts(static_cast<std::size_t>(days), 0);
    while (stmt.step()) {
        const auto ago = stmt.getInt(0);
        if (ago >= 0 && ago < days) counts[static_cast<std::size_t>(days - 1 - ago)] = static_cast<int>(stmt.getInt(1));
    }
    return counts;
}

std::vector<Book> LibraryService::popularBooks(int limit) const {
    std::string columns;
    for (std::string_view column : {"id", "isbn", "title", "author", "publisher", "price_cents", "total_copies",
                                    "available_copies", "category", "subject", "course"}) {
        if (!columns.empty()) columns += ", ";
        columns += "b.";
        columns += column;
    }
    auto stmt = db_->prepare("SELECT " + columns + R"sql(
        FROM books b LEFT JOIN loans l ON l.book_id = b.id
        GROUP BY b.id
        ORDER BY COUNT(l.id) DESC, b.title COLLATE NOCASE
        LIMIT ?)sql");
    stmt.bind(1, std::max(0, limit));
    std::vector<Book> books;
    while (stmt.step()) books.push_back(readBook(stmt));
    return books;
}

bool LibraryService::seedIfEmpty() {
    if (stats().titles > 0 || stats().students > 0) return false;

    const auto book = [](std::string isbn, std::string title, std::string author, std::string publisher,
                         std::int64_t cents, int copies) {
        Book b;
        b.isbn = std::move(isbn);
        b.title = std::move(title);
        b.author = std::move(author);
        b.publisher = std::move(publisher);
        b.priceCents = cents;
        b.totalCopies = copies;
        return b;
    };
    const auto textbook = [&](std::string isbn, std::string title, std::string author, std::string publisher,
                              std::int64_t cents, int copies, std::string subject, std::string course) {
        Book b = book(std::move(isbn), std::move(title), std::move(author), std::move(publisher), cents, copies);
        b.category = BookCategory::Textbook;
        b.subject = std::move(subject);
        b.course = std::move(course);
        return b;
    };

    const std::vector<Book> books = {
        textbook("9787121155352", "C++ Primer 中文版（第5版）", "Stanley B. Lippman", "电子工业出版社", 12800, 4,
                 "计算机科学", "面向对象程序设计"),
        textbook("9787111544937", "深入理解计算机系统（第3版）", "Randal E. Bryant", "机械工业出版社", 13900, 3,
                 "计算机科学", "计算机系统基础"),
        textbook("9787040396638", "高等数学（第七版）上册", "同济大学数学系", "高等教育出版社", 4760, 6, "数学",
                 "高等数学 A"),
        book("9787544253994", "百年孤独", "加西亚·马尔克斯", "南海出版公司", 3960, 2),
        book("9787020002207", "红楼梦", "曹雪芹", "人民文学出版社", 5970, 3),
        book("9787508647357", "人类简史", "尤瓦尔·赫拉利", "中信出版社", 6800, 2),
        book("9787115428028", "Python 编程：从入门到实践", "Eric Matthes", "人民邮电出版社", 8900, 1),
    };
    const std::vector<Student> students = {
        {5120255017, "周星可", "zhouxk@example.edu", 0},
        {5120255260, "王悠悠", "wangyy@example.edu", 0},
        {5120255101, "李明哲", "", 0},
        {5120255233, "陈思远", "chensy@example.edu", 0},
        {5120255318, "林小满", "linxm@example.edu", 0},
    };

    Transaction tx(*db_);
    for (const auto& b : books) addBook(b);
    for (const auto& s : students) addStudent(s);

    // Two weeks of history so the dashboard has something to show.
    // {student, isbn, borrowed days ago, loan days, returned days ago (<0 = still out)}
    struct History {
        std::int64_t student;
        const char* isbn;
        int borrowedAgo, loanDays, returnedAgo;
    };
    const History history[] = {
        {5120255017, "9787121155352", 1, 30, -1},  {5120255260, "9787544253994", 3, 14, -1},
        {5120255233, "9787111544937", 0, 30, -1},  {5120255318, "9787508647357", 20, 14, -1},  // overdue
        {5120255101, "9787040396638", 5, 30, -1},  {5120255318, "9787115428028", 9, 30, -1},
        {5120255017, "9787020002207", 13, 14, 4},  {5120255260, "9787121155352", 12, 30, 6},
        {5120255101, "9787544253994", 11, 14, 2},  {5120255233, "9787020002207", 8, 14, 1},
        {5120255101, "9787121155352", 7, 30, 3},   {5120255260, "9787508647357", 6, 14, 5},
        {5120255233, "9787040396638", 4, 30, 2},   {5120255017, "9787111544937", 2, 30, 0},
        {5120255318, "9787121155352", 10, 30, 7},  {5120255233, "9787544253994", 6, 14, 4},
        {5120255017, "9787508647357", 6, 14, 3},   {5120255318, "9787040396638", 6, 30, 1},
        {5120255260, "9787111544937", 2, 30, 1},   {5120255101, "9787020002207", 2, 14, 0},
        {5120255017, "9787040396638", 9, 30, 5},   {5120255260, "9787020002207", 13, 14, 8},
        {5120255233, "9787115428028", 12, 30, 10},
    };
    const Timestamp now = clock_();
    for (const auto& h : history) {
        const auto bookId = listBooks({.keyword = h.isbn}).front().id;
        const Timestamp borrowed = now - h.borrowedAgo * kSecondsPerDay - 3600 * (h.borrowedAgo % 5 + 1);
        db_->prepare("INSERT INTO loans (book_id, student_id, borrowed_at, due_at, returned_at) VALUES (?, ?, ?, ?, ?)")
            .bindAll(bookId, h.student, borrowed, borrowed + h.loanDays * kSecondsPerDay)
            .bind(5, h.returnedAgo < 0 ? std::optional<std::int64_t>{} : now - h.returnedAgo * kSecondsPerDay)
            .run();
        if (h.returnedAgo < 0) {
            db_->prepare("UPDATE books SET available_copies = available_copies - 1 WHERE id = ?").bind(1, bookId).run();
        }
    }
    tx.commit();
    return true;
}

}  // namespace library
