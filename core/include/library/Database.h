#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

struct sqlite3;
struct sqlite3_stmt;

namespace library {

class Statement;

/// RAII owner of a SQLite connection. Applies schema migrations on open.
class Database {
public:
    /// Opens (or creates) the database file. Use ":memory:" for tests.
    explicit Database(const std::string& path);
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    [[nodiscard]] Statement prepare(std::string_view sql) const;
    void execute(std::string_view sql) const;

    [[nodiscard]] std::int64_t lastInsertId() const;
    [[nodiscard]] int changes() const;

private:
    void migrate();

    sqlite3* handle_ = nullptr;
};

/// RAII prepared statement with a small fluent binding API.
/// Parameters are 1-based, columns are 0-based (mirrors SQLite).
class Statement {
public:
    Statement(sqlite3* db, std::string_view sql);
    ~Statement();

    Statement(Statement&& other) noexcept;
    Statement& operator=(Statement&&) = delete;
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    Statement& bind(int index, std::int64_t value);
    Statement& bind(int index, int value) { return bind(index, static_cast<std::int64_t>(value)); }
    Statement& bind(int index, double value);
    Statement& bind(int index, std::string_view value);
    Statement& bind(int index, const char* value) { return bind(index, std::string_view(value)); }
    Statement& bind(int index, const std::string& value) { return bind(index, std::string_view(value)); }
    Statement& bindNull(int index);
    template <typename T>
    Statement& bind(int index, const std::optional<T>& value) {
        return value ? bind(index, *value) : bindNull(index);
    }

    /// Binds all arguments to parameters 1..N in order.
    template <typename... Args>
    Statement& bindAll(const Args&... args) {
        int index = 0;
        (bind(++index, args), ...);
        return *this;
    }

    /// Advances to the next row. Returns false when the result set is exhausted.
    bool step();
    /// Executes a statement that produces no rows.
    void run();

    [[nodiscard]] std::int64_t getInt(int column) const;
    [[nodiscard]] double getDouble(int column) const;
    [[nodiscard]] std::string getText(int column) const;
    [[nodiscard]] bool isNull(int column) const;
    [[nodiscard]] std::optional<std::int64_t> getOptionalInt(int column) const;

private:
    sqlite3* db_;
    sqlite3_stmt* stmt_ = nullptr;
};

/// Scoped write transaction: rolls back unless commit() is called.
class Transaction {
public:
    explicit Transaction(const Database& db);
    ~Transaction();

    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;

    void commit();

private:
    const Database& db_;
    bool finished_ = false;
};

}  // namespace library
