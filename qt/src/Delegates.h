#pragma once

#include <QStyledItemDelegate>

namespace ui {

/// Leading visual for TwoLineDelegate, from Role::LeadingRole.
enum class Leading { None, Cover, Avatar };

/// Bold title + muted subtitle (Role::SubtitleRole), optionally preceded by a
/// generated book cover or avatar (Role::LeadingRole). Search matches
/// (Role::HighlightRole) are drawn in the accent colour.
class TwoLineDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};

/// Rounded pill; colour from Role::ToneRole. Danger badges get a status dot.
class BadgeDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};

/// Progress bar + "available/total" text; data from Role::StockRole (QPoint).
class StockDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};

/// Five slots filling up with the reader's active loans; Role::StockRole = QPoint(active, max).
class QuotaDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};

/// Paints the row background (hover / selected) consistently for every column,
/// so rows read as one unit. Used for plain-text columns.
class RowDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};

}  // namespace ui
