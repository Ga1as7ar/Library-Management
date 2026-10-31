#include "ApiRoutes.h"

#include <httplib/httplib.h>

#include <charconv>
#include <functional>
#include <optional>
#include <string>

#include "JsonMapping.h"
#include "library/Errors.h"
#include "library/LibraryService.h"

namespace library {

using nlohmann::json;
using Request = httplib::Request;
using Response = httplib::Response;

namespace {

int httpStatusFor(ErrorCode code) {
    switch (code) {
        case ErrorCode::Validation: return 400;
        case ErrorCode::NotFound: return 404;
        case ErrorCode::Conflict: return 409;
        case ErrorCode::BusinessRule: return 422;
        case ErrorCode::Database: break;
    }
    return 500;
}

void sendJson(Response& res, const json& body, int status = 200) {
    res.status = status;
    res.set_content(body.dump(), "application/json; charset=utf-8");
}

void sendError(Response& res, int status, const std::string& message) {
    sendJson(res, {{"error", message}}, status);
}

std::int64_t parseId(const std::string& text) {
    std::int64_t value = 0;
    const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (ec != std::errc{} || end != text.data() + text.size()) {
        throw LibraryError(ErrorCode::Validation, "无效的编号: " + text);
    }
    return value;
}

std::int64_t pathId(const Request& req) { return parseId(req.matches[1]); }

std::optional<std::int64_t> queryId(const Request& req, const char* key) {
    if (!req.has_param(key) || req.get_param_value(key).empty()) return std::nullopt;
    return parseId(req.get_param_value(key));
}

json body(const Request& req) {
    auto parsed = json::parse(req.body, nullptr, /*allow_exceptions=*/false);
    if (parsed.is_discarded() || !parsed.is_object()) throw LibraryError(ErrorCode::Validation, "请求体必须是 JSON 对象");
    return parsed;
}

LoanFilter parseLoanFilter(const std::string& text) {
    if (text == "active") return LoanFilter::Active;
    if (text == "overdue") return LoanFilter::Overdue;
    if (text == "returned") return LoanFilter::Returned;
    return LoanFilter::All;
}

/// Wraps a handler with locking and uniform error translation, so individual
/// routes contain only the happy path. The Router itself is a short-lived
/// registration helper: wrapped handlers capture only long-lived references.
class Router {
public:
    using Handler = std::function<void(const Request&, Response&)>;

    Router(httplib::Server& server, std::mutex& mutex) : server_(server), mutex_(mutex) {}

    void get(const std::string& pattern, Handler h) { server_.Get(pattern, wrap(std::move(h))); }
    void post(const std::string& pattern, Handler h) { server_.Post(pattern, wrap(std::move(h))); }
    void put(const std::string& pattern, Handler h) { server_.Put(pattern, wrap(std::move(h))); }
    void del(const std::string& pattern, Handler h) { server_.Delete(pattern, wrap(std::move(h))); }

private:
    httplib::Server::Handler wrap(Handler handler) {
        return [&mutex = mutex_, handler = std::move(handler)](const Request& req, Response& res) {
            try {
                std::scoped_lock lock(mutex);
                handler(req, res);
            } catch (const LibraryError& error) {
                sendError(res, httpStatusFor(error.code()), error.what());
            } catch (const json::exception& error) {
                sendError(res, 400, std::string("JSON 字段类型错误: ") + error.what());
            } catch (const std::exception& error) {
                sendError(res, 500, std::string("服务器内部错误: ") + error.what());
            }
        };
    }

    httplib::Server& server_;
    std::mutex& mutex_;
};

}  // namespace

void registerApiRoutes(httplib::Server& server, LibraryService& service, std::mutex& mutex) {
    Router api(server, mutex);
    constexpr const char* kId = R"((\d+))";

    // ---------------------------------------------------------------- stats
    api.get("/api/stats", [&](const Request&, Response& res) { sendJson(res, service.stats()); });
    // ?days=14&tz=480  (tz = minutes east of UTC, i.e. -Date.getTimezoneOffset())
    api.get("/api/stats/activity", [&](const Request& req, Response& res) {
        const int days = req.has_param("days") ? static_cast<int>(parseId(req.get_param_value("days"))) : 14;
        const int tzMinutes = req.has_param("tz") ? std::stoi(req.get_param_value("tz")) : 0;
        sendJson(res, service.borrowActivity(days, tzMinutes * 60));
    });
    api.get("/api/stats/popular", [&](const Request& req, Response& res) {
        const int limit = req.has_param("limit") ? static_cast<int>(parseId(req.get_param_value("limit"))) : 5;
        sendJson(res, service.popularBooks(std::min(limit, 50)));
    });

    // ---------------------------------------------------------------- books
    api.get("/api/books", [&](const Request& req, Response& res) {
        BookQuery query;
        query.keyword = req.get_param_value("q");
        query.category = parseBookCategory(req.get_param_value("category"));
        query.onlyAvailable = req.get_param_value("available") == "true";
        sendJson(res, service.listBooks(query));
    });
    api.get(std::string("/api/books/") + kId,
            [&](const Request& req, Response& res) { sendJson(res, service.getBook(pathId(req))); });
    api.post("/api/books", [&](const Request& req, Response& res) {
        sendJson(res, service.addBook(body(req).get<Book>()), 201);
    });
    api.put(std::string("/api/books/") + kId, [&](const Request& req, Response& res) {
        auto book = body(req).get<Book>();
        book.id = pathId(req);
        sendJson(res, service.updateBook(book));
    });
    api.del(std::string("/api/books/") + kId, [&](const Request& req, Response& res) {
        service.removeBook(pathId(req));
        res.status = 204;
    });

    // ---------------------------------------------------------------- students
    api.get("/api/students", [&](const Request& req, Response& res) {
        sendJson(res, service.listStudents(req.get_param_value("q")));
    });
    api.get(std::string("/api/students/") + kId,
            [&](const Request& req, Response& res) { sendJson(res, service.getStudent(pathId(req))); });
    api.post("/api/students", [&](const Request& req, Response& res) {
        sendJson(res, service.addStudent(body(req).get<Student>()), 201);
    });
    api.put(std::string("/api/students/") + kId, [&](const Request& req, Response& res) {
        auto student = body(req).get<Student>();
        student.id = pathId(req);
        sendJson(res, service.updateStudent(student));
    });
    api.del(std::string("/api/students/") + kId, [&](const Request& req, Response& res) {
        service.removeStudent(pathId(req));
        res.status = 204;
    });

    // ---------------------------------------------------------------- loans
    api.get("/api/loans", [&](const Request& req, Response& res) {
        LoanQuery query;
        query.studentId = queryId(req, "studentId");
        query.bookId = queryId(req, "bookId");
        query.filter = parseLoanFilter(req.get_param_value("status"));
        sendJson(res, service.listLoans(query));
    });
    api.post("/api/loans", [&](const Request& req, Response& res) {
        const json payload = body(req);
        const int days = payload.value("days", LibraryService::kDefaultLoanDays);
        sendJson(res,
                 service.borrowBook(payload.at("studentId").get<std::int64_t>(),
                                    payload.at("bookId").get<std::int64_t>(), days),
                 201);
    });
    api.post(std::string("/api/loans/") + kId + "/return",
             [&](const Request& req, Response& res) { sendJson(res, service.returnBook(pathId(req))); });

    // Unknown /api paths return JSON, not the SPA shell.
    server.set_error_handler([](const Request& req, Response& res) {
        if (req.path.rfind("/api/", 0) == 0 && res.body.empty()) sendError(res, res.status, "接口不存在");
    });
}

}  // namespace library
