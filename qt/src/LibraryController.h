#pragma once

#include <QObject>
#include <QString>
#include <functional>
#include <memory>
#include <optional>

#include "library/LibraryService.h"

namespace ui {

/// Qt-side façade over LibraryService.
///
/// - Read operations pass straight through (`service()`).
/// - Mutations go through `run()`, which converts LibraryError into a
///   user-visible message and broadcasts `dataChanged()` so every page refreshes.
///   Pages therefore never need to know about each other.
class LibraryController : public QObject {
    Q_OBJECT

public:
    explicit LibraryController(std::unique_ptr<library::LibraryService> service, QObject* parent = nullptr);

    [[nodiscard]] const library::LibraryService& service() const { return *service_; }
    [[nodiscard]] library::Timestamp now() const { return library::LibraryService::systemClock(); }

    /// Runs a mutation. On success emits dataChanged() and returns the result;
    /// on failure returns std::nullopt and writes the message to `error`.
    template <typename Fn>
    auto run(Fn&& mutation, QString* error = nullptr) -> std::optional<decltype(mutation(std::declval<library::LibraryService&>()))> {
        try {
            auto result = mutation(*service_);
            emit dataChanged();
            return result;
        } catch (const std::exception& e) {
            if (error) *error = QString::fromUtf8(e.what());
            return std::nullopt;
        }
    }

signals:
    void dataChanged();
    /// Requests a transient notification (shown by MainWindow's toast).
    void notify(const QString& message, bool success = true);

private:
    std::unique_ptr<library::LibraryService> service_;
};

}  // namespace ui
