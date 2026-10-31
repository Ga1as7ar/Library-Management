#include "Delegates.h"

#include <QAbstractItemView>
#include <QPainter>
#include <QPainterPath>

#include "Common.h"
#include "Painted.h"
#include "Theme.h"

namespace ui {

namespace {

constexpr int kPadding = 14;

/// Row background + bottom separator, shared by all delegates so hover and
/// selection look identical across columns (stylesheet item rules are bypassed).
void paintRow(QPainter* painter, const QStyleOptionViewItem& option) {
    const auto& p = Theme::instance().palette();
    painter->save();
    if (option.state & QStyle::State_Selected) {
        painter->fillRect(option.rect, p.accentSoft);
    } else if (option.state & QStyle::State_MouseOver) {
        painter->fillRect(option.rect, p.surface2);
    }
    painter->setPen(p.border);
    painter->drawLine(option.rect.bottomLeft(), option.rect.bottomRight());
    painter->restore();
}

QFont scaled(const QFont& base, qreal delta, QFont::Weight weight = QFont::Normal) {
    QFont font = base;
    font.setPointSizeF(base.pointSizeF() + delta);
    font.setWeight(weight);
    return font;
}

/// Draws `text` elided into `rect`, colouring the first case-insensitive match of `needle`.
void drawHighlighted(QPainter* painter, const QRect& rect, const QString& text, const QString& needle,
                     const QColor& normal, const QColor& accent) {
    const QFontMetrics metrics(painter->font());
    const QString shown = metrics.elidedText(text, Qt::ElideRight, rect.width());
    const int at = needle.isEmpty() ? -1 : int(shown.indexOf(needle, 0, Qt::CaseInsensitive));
    if (at < 0) {
        painter->setPen(normal);
        painter->drawText(rect, Qt::AlignLeft | Qt::AlignVCenter, shown);
        return;
    }
    int x = rect.left();
    const auto draw = [&](const QString& part, const QColor& color) {
        painter->setPen(color);
        painter->drawText(QRect(x, rect.top(), rect.right() - x, rect.height()), Qt::AlignLeft | Qt::AlignVCenter, part);
        x += metrics.horizontalAdvance(part);
    };
    draw(shown.left(at), normal);
    draw(shown.mid(at, needle.size()), accent);
    draw(shown.mid(at + needle.size()), normal);
}

}  // namespace

// ------------------------------------------------------------ Row

void RowDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    paintRow(painter, option);
    const auto& p = Theme::instance().palette();
    painter->save();
    QFont font = option.font;
    if (const QVariant f = index.data(Qt::FontRole); f.isValid()) font = f.value<QFont>();
    painter->setFont(font);
    const QVariant fg = index.data(Qt::ForegroundRole);
    painter->setPen(fg.isValid() ? fg.value<QBrush>().color() : p.text);
    const auto alignment = index.data(Qt::TextAlignmentRole).isValid()
                               ? Qt::Alignment(index.data(Qt::TextAlignmentRole).toInt())
                               : Qt::Alignment(Qt::AlignLeft | Qt::AlignVCenter);
    painter->drawText(option.rect.adjusted(kPadding, 0, -kPadding, 0), alignment, index.data().toString());
    painter->restore();
}

// ------------------------------------------------------------ TwoLine

void TwoLineDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    paintRow(painter, option);
    const auto& p = Theme::instance().palette();
    QRect area = option.rect.adjusted(kPadding, 0, -kPadding, 0);
    const QString title = index.data(Qt::DisplayRole).toString();
    const QString subtitle = index.data(SubtitleRole).toString();
    const QString needle = index.data(HighlightRole).toString();

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    const auto leading = static_cast<Leading>(index.data(LeadingRole).toInt());
    const bool hovered = option.state & QStyle::State_MouseOver;
    if (leading == Leading::Cover) {
        const QRectF cover(area.left(), area.center().y() - 22, 32, 44);
        if (hovered) {  // tilt the cover a little, like pulling a book from a shelf
            painter->save();
            painter->translate(cover.center());
            painter->rotate(-5);
            painter->translate(-cover.center());
            paintCover(*painter, cover.translated(1, -2), title);
            painter->restore();
        } else {
            paintCover(*painter, cover, title);
        }
        area.setLeft(area.left() + 44);
    } else if (leading == Leading::Avatar) {
        paintAvatar(*painter, QRectF(area.left(), area.center().y() - 17, 34, 34), title);
        area.setLeft(area.left() + 46);
    }

    const QFont titleFont = scaled(option.font, 0, QFont::DemiBold);
    const QFont subFont = scaled(option.font, -1.5);
    const QFontMetrics titleMetrics(titleFont), subMetrics(subFont);
    const int total = titleMetrics.height() + (subtitle.isEmpty() ? 0 : subMetrics.height() + 3);
    int y = area.top() + (area.height() - total) / 2;

    painter->setFont(titleFont);
    drawHighlighted(painter, QRect(area.left(), y, area.width(), titleMetrics.height()), title, needle, p.text, p.accent);
    if (!subtitle.isEmpty()) {
        y += titleMetrics.height() + 3;
        painter->setFont(subFont);
        drawHighlighted(painter, QRect(area.left(), y, area.width(), subMetrics.height()), subtitle, needle, p.text3, p.accent);
    }
    painter->restore();
}

// ------------------------------------------------------------ Badge

void BadgeDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    paintRow(painter, option);
    const QString text = index.data(Qt::DisplayRole).toString();
    if (text.isEmpty()) return;

    const auto& p = Theme::instance().palette();
    const auto tone = static_cast<Tone>(index.data(ToneRole).toInt());
    QColor fg = p.text2, bg = p.surfaceHover;
    switch (tone) {
        case Tone::Accent: fg = p.accent; bg = p.accentSoft; break;
        case Tone::Warning: fg = p.warning; bg = p.warningSoft; break;
        case Tone::Danger: fg = p.danger; bg = p.dangerSoft; break;
        case Tone::Neutral: break;
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    const QFont font = scaled(option.font, -1, QFont::Medium);
    const QFontMetrics metrics(font);
    const int dot = tone == Tone::Danger ? 12 : 0;
    const int height = 22;
    const int width = metrics.horizontalAdvance(text) + 20 + dot;
    const QRect pill(option.rect.left() + kPadding, option.rect.center().y() - height / 2 + 1, width, height);

    QPainterPath path;
    path.addRoundedRect(pill, height / 2.0, height / 2.0);
    painter->fillPath(path, bg);
    if (dot) {
        QColor halo = fg;
        halo.setAlpha(60);
        painter->setPen(Qt::NoPen);
        painter->setBrush(halo);
        painter->drawEllipse(QPointF(pill.left() + 12, pill.center().y() + 0.5), 4.5, 4.5);
        painter->setBrush(fg);
        painter->drawEllipse(QPointF(pill.left() + 12, pill.center().y() + 0.5), 2.6, 2.6);
    }
    painter->setFont(font);
    painter->setPen(fg);
    painter->drawText(pill.adjusted(dot, 0, 0, 0), Qt::AlignCenter, text);
    painter->restore();
}

// ------------------------------------------------------------ Stock

void StockDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    paintRow(painter, option);
    const QPoint stock = index.data(StockRole).toPoint();  // x = available, y = total
    const auto& p = Theme::instance().palette();

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    const QString label = QStringLiteral("%1/%2").arg(stock.x()).arg(stock.y());
    const QFontMetrics metrics(option.font);
    const int textWidth = metrics.horizontalAdvance(QStringLiteral("00/00"));
    const QRect area = option.rect.adjusted(kPadding, 0, -kPadding, 0);
    const QRectF track(area.left(), area.center().y() - 2.5, area.width() - textWidth - 12, 6);

    QPainterPath trackPath;
    trackPath.addRoundedRect(track, 3, 3);
    painter->fillPath(trackPath, p.surfaceHover);
    QRectF fill = track;
    QPainterPath fillPath;
    if (stock.x() > 0 && stock.y() > 0) {
        fill.setWidth(track.width() * stock.x() / stock.y());
        QLinearGradient g(fill.topLeft(), fill.topRight());
        g.setColorAt(0, p.accent);
        g.setColorAt(1, p.accentHover);
        fillPath.addRoundedRect(fill, 3, 3);
        painter->fillPath(fillPath, g);
    } else {
        fillPath.addRoundedRect(fill, 3, 3);
        painter->fillPath(fillPath, p.danger);
    }
    painter->setPen(p.text2);
    painter->drawText(QRect(int(track.right()) + 12, area.top(), textWidth, area.height()), Qt::AlignRight | Qt::AlignVCenter, label);
    painter->restore();
}

// ------------------------------------------------------------ Quota

void QuotaDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    paintRow(painter, option);
    const QPoint quota = index.data(StockRole).toPoint();  // x = active, y = max
    const auto& p = Theme::instance().palette();
    const QColor on = quota.x() >= quota.y() ? p.warning : p.accent;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    qreal x = option.rect.left() + kPadding;
    const qreal y = option.rect.center().y() - 3;
    for (int i = 0; i < quota.y(); ++i) {
        QPainterPath slot;
        slot.addRoundedRect(QRectF(x, y, 15, 6), 3, 3);
        painter->fillPath(slot, i < quota.x() ? on : p.surfaceHover);
        x += 18;
    }
    painter->setPen(p.text3);
    painter->setFont(scaled(option.font, -1));
    painter->drawText(QRectF(x + 6, option.rect.top(), 60, option.rect.height()), Qt::AlignLeft | Qt::AlignVCenter,
                      QStringLiteral("%1 / %2").arg(quota.x()).arg(quota.y()));
    painter->restore();
}

}  // namespace ui
