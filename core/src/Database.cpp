#include "library/Database.h"

#include <sqlite3.h>

#include <array>

#include "library/Errors.h"

namespace library {

namespace {

[[noreturn]] void throwSqlite(sqlite3* db, const std::string& context) {
    const int code = sqlite3_extended_errcode(db);
    const std::string detail = sqlite3_errmsg(db);
    if (code == SQLITE_CONSTRAINT_UNIQUE || code == SQLITE_CONSTRAINT_PRIMARYKEY) {
        throw LibraryError(ErrorCode::Conflict, context + ": " + detail);
    }
    throw LibraryError(ErrorCode::Database, context + ": " + detail);
}

/// Each entry upgrades the schema by exactly one version. Append only — never edit.
constexpr std::array kMigrations = {
    R"sql(
    CREATE TABLE books (
        id               INTEGER PRIMARY KEY AUTOINCREMENT,
        isbn             TEXT    NOT NULL UNIQUE,
        title            TEXT    NOT NULL,
        author           TEXT    NOT NULL DEFAULT '',
        publisher        TEXT    NOT NULL DEFAULT '',
        price_cents      INTEGER NOT NULL DEFAULT 0 CHECK (price_cents >= 0),
        total_copies     INTEGER NOT NULL CHECK (total_copies >= 0),
        available_copies INTEGER NOT NULL CHECK (available_copies BETWEEN 0 AND total_copies),
        category         TEXT    NOT NULL DEFAULT 'general' CHECK (category IN ('general', 'textbook')),
        subject          TEXT    NOT NULL DEFAULT '',
        course           TEXT    NOT NULL DEFAULT '',
        created_at       INTEGER NOT NULL DEFAULT (unixepoch())
    );

    CREATE TABLE students (
        id         INTEGER PRIMARY KEY,
        name       TEXT    NOT NULL,
        email      TEXT    NOT NULL DEFAULT '',
        created_at INTEGER NOT NULL DEFAULT (unixepoch())
    );

    CREATE TABLE loans (
        id          INTEGER PRIMARY KEY AUTOINCREMENT,
        book_id     INTEGER NOT NULL REFERENCES books(id)    ON DELETE RESTRICT,
        student_id  INTEGER NOT NULL REFERENCES students(id) ON DELETE RESTRICT,
        borrowed_at INTEGER NOT NULL,
        due_at      INTEGER NOT NULL,
        returned_at INTEGER
    );

    CREATE INDEX idx_loans_student ON loans(student_id);
    CREATE INDEX idx_loans_book    ON loans(book_id);
    CREATE INDEX idx_loans_active  ON loans(returned_at) WHERE returned_at IS NULL;
    )sql",
};

}  // namespace

// ---------------------------------------------------------------- Database

Database::Database(const std::string& path) {
    if (sqlite3_open(path.c_str(), &handle_) != SQLITE_OK) {
        const std::string message = handle_ ? sqlite3_errmsg(handle_) : "out of memory";
        sqlite3_close(handle_);
        throw LibraryError(ErrorCode::Database, "无法打开数据库 " + path + ": " + message);
    }
    sqlite3_busy_timeout(handle_, 3000);
    execute("PRAGMA foreign_keys = ON;");
    execute("PRAGMA journal_mode = WAL;");
    migrate();
}

Database::~Database() { sqlite3_close_v2(handle_); }

Statement Database::prepare(std::string_view sql) const { return {handle_, sql}; }

void Database::execute(std::string_view sql) const {
    char* error = nullptr;
    if (sqlite3_exec(handle_, std::string(sql).c_str(), nullptr, nullptr, &error) != SQLITE_OK) {
        const std::string message = error ? error : "unknown error";
        sqlite3_free(error);
        throw LibraryError(ErrorCode::Database, message);
    }
}

std::int64_t Database::lastInsertId() const { return sqlite3_last_insert_rowid(handle_); }

int Database::changes() const { return sqlite3_changes(handle_); }

void Database::migrate() {
    auto versionQuery = prepare("PRAGMA user_version;");
    versionQuery.step();
    const auto current = static_cast<std::size_t>(versionQuery.getInt(0));

    for (std::size_t version = current; version < kMigrations.size(); ++version) {
        Transaction tx(*this);
        execute(kMigrations[version]);
        execute("PRAGMA user_version = " + std::to_string(version + 1) + ";");
        tx.commit();
    }
}

// ---------------------------------------------------------------- Statement

Statement::Statement(sqlite3* db, std::string_view sql) : db_(db) {
    if (sqlite3_prepare_v2(db_, sql.data(), static_cast<int>(sql.size()), &stmt_, nullptr) != SQLITE_OK) {
        throwSqlite(db_, "SQL 预编译失败");
    }
}

Statement::~Statement() { sqlite3_finalize(stmt_); }

Statement::Statement(Statement&& other) noexcept : db_(other.db_), stmt_(other.stmt_) {
    other.stmt_ = nullptr;
}

Statement& Statement::bind(int index, std::int64_t value) {
    if (sqlite3_bind_int64(stmt_, index, value) != SQLITE_OK) throwSqlite(db_, "参数绑定失败");
    return *this;
}

Statement& Statement::bind(int index, double value) {
    if (sqlite3_bind_double(stmt_, index, value) != SQLITE_OK) throwSqlite(db_, "参数绑定失败");
    return *this;
}

Statement& Statement::bind(int index, std::string_view value) {
    if (sqlite3_bind_text(stmt_, index, value.data(), static_cast<int>(value.size()), SQLITE_TRANSIENT) !=
        SQLITE_OK) {
        throwSqlite(db_, "参数绑定失败");
    }
    return *this;
}

Statement& Statement::bindNull(int index) {
    if (sqlite3_bind_null(stmt_, index) != SQLITE_OK) throwSqlite(db_, "参数绑定失败");
    return *this;
}

bool Statement::step() {
    switch (sqlite3_step(stmt_)) {
        case SQLITE_ROW: return true;
        case SQLITE_DONE: return false;
        default: throwSqlite(db_, "SQL 执行失败");
    }
}

void Statement::run() {
    while (step()) {
    }
}

std::int64_t Statement::getInt(int column) const { return sqlite3_column_int64(stmt_, column); }

double Statement::getDouble(int column) const { return sqlite3_column_double(stmt_, column); }

std::string Statement::getText(int column) const {
    const auto* text = sqlite3_column_text(stmt_, column);
    return text ? reinterpret_cast<const char*>(text) : std::string{};
}

bool Statement::isNull(int column) const { return sqlite3_column_type(stmt_, column) == SQLITE_NULL; }

std::optional<std::int64_t> Statement::getOptionalInt(int column) const {
    if (isNull(column)) return std::nullopt;
    return getInt(column);
}

// ---------------------------------------------------------------- Transaction

Transaction::Transaction(const Database& db) : db_(db) { db_.execute("BEGIN IMMEDIATE;"); }

Transaction::~Transaction() {
    if (!finished_) {
        try {
            db_.execute("ROLLBACK;");
        } catch (...) {
            // Destructors must not throw; a failed rollback leaves SQLite to recover.
        }
    }
}

void Transaction::commit() {
    db_.execute("COMMIT;");
    finished_ = true;
}

}  // namespace library
