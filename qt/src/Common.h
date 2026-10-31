#pragma once

#include <QDateTime>
#include <QString>
#include <string>

#include "library/Models.h"

/// Small conversions shared by every Qt view. Domain types stay std::string;
/// translation to QString happens only at the UI boundary.
namespace ui {

inline QString qs(const std::string& text) { return QString::fromStdString(text); }
inline std::string ss(const QString& text) { return text.trimmed().toStdString(); }

inline QString formatDate(library::Timestamp seconds) {
    return QDateTime::fromSecsSinceEpoch(seconds).toString(QStringLiteral("yyyy/MM/dd"));
}

inline QString formatPrice(std::int64_t cents) { return QStringLiteral("¥%1").arg(cents / 100.0, 0, 'f', 2); }

/// Whole days until `due` (ceil), negative once overdue — matches the web UI.
inline int daysUntil(library::Timestamp due, library::Timestamp now) {
    constexpr library::Timestamp kDay = 86400;
    const auto diff = due - now;
    return static_cast<int>(diff >= 0 ? (diff + kDay - 1) / kDay : -((-diff) / kDay));
}

/// Custom item-data roles understood by the delegates in Delegates.h.
enum Role {
    SortRole = Qt::UserRole + 1,  ///< Raw value used by the sort proxy.
    SubtitleRole,                 ///< Second line for TwoLineDelegate.
    ToneRole,                     ///< ui::Tone for BadgeDelegate.
    StockRole,                    ///< QPoint(available, total) for Stock/QuotaDelegate.
    LeadingRole,                  ///< ui::Leading (cover / avatar) for TwoLineDelegate.
    HighlightRole,                ///< Search term to emphasise in TwoLineDelegate.
};

enum class Tone { Neutral, Accent, Warning, Danger };

}  // namespace ui
