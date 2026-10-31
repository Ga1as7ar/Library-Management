#include "Painted.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <QtMath>
#include <algorithm>

#include "Theme.h"

namespace ui {

namespace {

/// HSL-ish colour from hue with fixed lightness/saturation tuned per theme.
QColor tint(int hue, qreal saturation, qreal lightness) { return QColor::fromHslF(hue / 360.0, saturation, lightness); }

QFont withSize(QFont font, qreal pointSize, QFont::Weight weight = QFont::Normal) {
    font.setPointSizeF(pointSize);
    font.setWeight(weight);
    return font;
}

}  // namespace

int hueOf(const QString& text) {
    quint32 hash = 0;
    for (const auto ch : text.toUcs4()) hash = hash * 31 + ch;
    return int(hash % 360);
}

QString initialOf(const QString& text) {
    const auto chars = text.trimmed().toUcs4();
    return chars.isEmpty() ? QStringLiteral("?") : QString::fromUcs4(reinterpret_cast<const char32_t*>(chars.constData()), 1).toUpper();
}

void paintCover(QPainter& painter, const QRectF& rect, const QString& title) {
    const bool dark = Theme::instance().mode() == Theme::Mode::Dark;
    const int hue = hueOf(title);
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);

    // Soft tinted shadow.
    QPainterPath shadow;
    shadow.addRoundedRect(rect.translated(0, 2), 4, 4);
    painter.fillPath(shadow, QColor(0, 0, 0, dark ? 70 : 28));

    // Body: diagonal gradient.
    QPainterPath body;
    body.addRoundedRect(rect, 3, 3);
    QLinearGradient gradient(rect.topLeft(), rect.bottomRight());
    gradient.setColorAt(0, tint(hue, .30, dark ? .46 : .50));
    gradient.setColorAt(1, tint(hue, .36, dark ? .28 : .32));
    painter.fillPath(body, gradient);

    // Spine + highlight line.
    painter.fillRect(QRectF(rect.left(), rect.top(), 3, rect.height()), QColor(0, 0, 0, 55));
    painter.fillRect(QRectF(rect.left() + 3, rect.top(), 1, rect.height()), QColor(255, 255, 255, 40));
    // Page edge on the right.
    painter.fillRect(QRectF(rect.right() - 1.5, rect.top() + 3, 1.5, rect.height() - 6), QColor(255, 255, 255, 70));

    painter.setPen(QColor(255, 255, 255, 235));
    painter.setFont(withSize(painter.font(), rect.height() * 0.24, QFont::Bold));
    painter.drawText(rect.adjusted(6, 0, 0, -4), Qt::AlignLeft | Qt::AlignBottom, initialOf(title));
    painter.restore();
}

void paintAvatar(QPainter& painter, const QRectF& rect, const QString& name) {
    const bool dark = Theme::instance().mode() == Theme::Mode::Dark;
    const int hue = hueOf(name);
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(tint(hue, dark ? .25 : .55, dark ? .26 : .91));
    painter.drawEllipse(rect);
    painter.setPen(tint(hue, .45, dark ? .82 : .38));
    painter.setFont(withSize(painter.font(), rect.height() * 0.30, QFont::DemiBold));
    const auto chars = name.toUcs4();
    const QString label = chars.size() <= 2 ? name : QString::fromUcs4(reinterpret_cast<const char32_t*>(chars.constData()) + chars.size() - 2, 2);
    painter.drawText(rect, Qt::AlignCenter, label);
    painter.restore();
}

// ====================================================================== HeroBanner

HeroBanner::HeroBanner(QWidget* parent) : QWidget(parent), tick_(new QTimer(this)) {
    setMinimumHeight(156);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setAttribute(Qt::WA_OpaquePaintEvent, false);

    // ~30 fps drift is plenty for a slow aurora and keeps CPU usage negligible.
    tick_->setInterval(33);
    connect(tick_, &QTimer::timeout, this, [this] {
        phase_ += 0.006;
        update();
    });
    tick_->start();

    introAnimation_.setDuration(1100);
    introAnimation_.setStartValue(0.0);
    introAnimation_.setEndValue(1.0);
    introAnimation_.setEasingCurve(QEasingCurve::OutBack);
    connect(&introAnimation_, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
        intro_ = v.toReal();
        update();
    });
}

void HeroBanner::setText(const QString& eyebrow, const QString& title, const QString& subtitle) {
    eyebrow_ = eyebrow;
    title_ = title;
    subtitle_ = subtitle;
    update();
}

void HeroBanner::setAction(QWidget* action) {
    action_ = action;
    action_->setParent(this);
    action_->show();
    resizeEvent(nullptr);
}

void HeroBanner::replayIntro() {
    introAnimation_.stop();
    introAnimation_.start();
}

void HeroBanner::resizeEvent(QResizeEvent*) {
    if (!action_) return;
    action_->adjustSize();
    action_->move(width() - action_->width() - 32, (height() - action_->height()) / 2);
}

void HeroBanner::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r = rect();
    const bool dark = Theme::instance().mode() == Theme::Mode::Dark;

    QPainterPath clip;
    clip.addRoundedRect(r, 18, 18);
    p.setClipPath(clip);

    // Base gradient.
    QLinearGradient base(r.topLeft(), r.bottomRight());
    base.setColorAt(0, dark ? QColor("#0d2a23") : QColor("#0d3f33"));
    base.setColorAt(0.55, dark ? QColor("#12382f") : QColor("#185a4a"));
    base.setColorAt(1, dark ? QColor("#1a5245") : QColor("#1f6f5c"));
    p.fillRect(r, base);

    // Aurora blobs drifting on Lissajous paths.
    const auto blob = [&](qreal cx, qreal cy, qreal radius, QColor color) {
        QRadialGradient g(QPointF(cx, cy), radius);
        g.setColorAt(0, color);
        color.setAlpha(0);
        g.setColorAt(1, color);
        p.fillRect(r, g);
    };
    const qreal w = r.width(), h = r.height(), t = phase_;
    blob(w * (0.30 + 0.08 * qSin(t * 1.3)), h * (0.40 + 0.25 * qCos(t)), w * 0.32, QColor(120, 230, 190, 70));
    blob(w * (0.70 + 0.06 * qCos(t * 0.9)), h * (0.60 + 0.20 * qSin(t * 1.1)), w * 0.28, QColor(240, 200, 120, 46));
    blob(w * (0.55 + 0.10 * qSin(t * 0.7 + 1)), h * (0.15 + 0.15 * qCos(t * 1.4)), w * 0.24, QColor(90, 200, 220, 40));

    // Dotted texture fading in towards the right.
    for (int x = 12; x < w; x += 18) {
        const int alpha = int(std::clamp((x / w - 0.25) * 60, 0.0, 34.0));
        if (!alpha) continue;
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, alpha));
        for (int y = 12; y < h; y += 18) p.drawEllipse(QPointF(x, y), 0.9, 0.9);
    }

    // Little bookshelf rising from the bottom edge.
    const int heights[] = {46, 62, 54, 70, 40, 58, 66};
    qreal x = w - 380;
    for (int i = 0; i < 7; ++i) {
        const qreal local = std::clamp(intro_ * 1.6 - i * 0.09, 0.0, 1.2);
        const qreal bookH = heights[i] * std::min(local, 1.08);
        QRectF book(x, h - bookH + 2, 17, bookH);
        p.save();
        if (i % 3 == 2) {  // a leaning book
            p.translate(book.bottomLeft());
            p.rotate(-8);
            p.translate(-book.bottomLeft());
        }
        QPainterPath spine;
        spine.addRoundedRect(book, 3, 3);
        p.fillPath(spine, QColor(255, 255, 255, 30));
        p.setPen(QPen(QColor(255, 255, 255, 46), 1));
        p.drawPath(spine);
        p.restore();
        x += 22;
    }

    // Text.
    p.setClipping(false);
    const int left = 32;
    p.setPen(QColor(242, 251, 247, 185));
    p.setFont(withSize(font(), 11.5));
    p.drawText(QRectF(left, 30, w - 300, 20), Qt::AlignLeft | Qt::AlignVCenter, eyebrow_);
    p.setPen(QColor(242, 251, 247));
    p.setFont(withSize(font(), 24, QFont::DemiBold));
    p.drawText(QRectF(left, 52, w - 300, 44), Qt::AlignLeft | Qt::AlignVCenter, title_);
    p.setPen(QColor(242, 251, 247, 200));
    p.setFont(withSize(font(), 13));
    p.drawText(QRectF(left, 100, w - 300, 24), Qt::AlignLeft | Qt::AlignVCenter, subtitle_);
}

// ====================================================================== ActivityChart

ActivityChart::ActivityChart(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    setMinimumHeight(190);
    animation_.setDuration(1000);
    animation_.setStartValue(0.0);
    animation_.setEndValue(1.0);
    animation_.setEasingCurve(QEasingCurve::Linear);
    connect(&animation_, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
        progress_ = v.toReal();
        update();
    });
}

void ActivityChart::setData(const QList<int>& counts, const QStringList& labels) {
    counts_ = counts;
    labels_ = labels;
    animation_.stop();
    animation_.start();
}

QRectF ActivityChart::barRect(int index, qreal progress) const {
    const int n = std::max<qsizetype>(1, counts_.size());
    const qreal gap = 7;
    const QRectF plot = QRectF(rect()).adjusted(0, 28, 0, -22);
    const qreal barW = (plot.width() - gap * (n - 1)) / n;
    const int max = std::max(1, *std::max_element(counts_.begin(), counts_.end()));
    // Each bar starts slightly after the previous one, with an overshooting ease.
    const qreal local = std::clamp(progress * 1.6 - index * 0.04, 0.0, 1.0);
    const qreal eased = QEasingCurve(QEasingCurve::OutBack).valueForProgress(local);
    const qreal full = std::max(4.0, plot.height() * counts_[index] / max);
    const qreal barH = full * eased;
    return {plot.left() + index * (barW + gap), plot.bottom() - barH, barW, barH};
}

void ActivityChart::paintEvent(QPaintEvent*) {
    if (counts_.isEmpty()) return;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const auto& pal = Theme::instance().palette();
    const int last = int(counts_.size()) - 1;

    // Baseline grid.
    const QRectF plot = QRectF(rect()).adjusted(0, 28, 0, -22);
    p.setPen(QPen(pal.border, 1, Qt::DashLine));
    for (int i = 1; i <= 3; ++i) p.drawLine(QPointF(plot.left(), plot.top() + plot.height() * i / 4.0), QPointF(plot.right(), plot.top() + plot.height() * i / 4.0));

    for (int i = 0; i <= last; ++i) {
        const QRectF bar = barRect(i, progress_);
        if (bar.height() <= 0) continue;
        QPainterPath path;
        path.addRoundedRect(bar, std::min(6.0, bar.width() / 3), std::min(6.0, bar.width() / 3));
        QLinearGradient g(bar.topLeft(), bar.bottomLeft());
        if (counts_[i] == 0) {
            g.setColorAt(0, pal.surfaceHover);
            g.setColorAt(1, pal.surfaceHover);
        } else if (i == last) {
            g.setColorAt(0, QColor("#e8b765"));
            g.setColorAt(1, QColor("#c98a2b"));
        } else {
            QColor top = pal.accentHover, bottom = pal.accent;
            if (hovered_ != -1 && hovered_ != i) {
                top.setAlphaF(0.55);
                bottom.setAlphaF(0.55);
            }
            g.setColorAt(0, top);
            g.setColorAt(1, bottom);
        }
        p.fillPath(path, g);

        if (i % 2 == last % 2 && i < labels_.size()) {
            p.setPen(pal.text3);
            p.setFont(withSize(font(), 10));
            p.drawText(QRectF(bar.center().x() - 20, rect().bottom() - 18, 40, 16), Qt::AlignCenter, labels_[i]);
        }
    }

    // Tooltip bubble.
    if (hovered_ >= 0 && hovered_ <= last) {
        const QRectF bar = barRect(hovered_, 1);
        const QString text = QStringLiteral("%1 · %2 次").arg(labels_.value(hovered_)).arg(counts_[hovered_]);
        p.setFont(withSize(font(), 11, QFont::Medium));
        const qreal tw = QFontMetricsF(p.font()).horizontalAdvance(text) + 18;
        QRectF bubble(bar.center().x() - tw / 2, bar.top() - 32, tw, 24);
        bubble.moveLeft(std::clamp(bubble.left(), 0.0, width() - tw));
        bubble.moveTop(std::max(0.0, bubble.top()));
        QPainterPath path;
        path.addRoundedRect(bubble, 6, 6);
        p.fillPath(path, pal.text);
        p.setPen(pal.surface);
        p.drawText(bubble, Qt::AlignCenter, text);
    }
}

void ActivityChart::mouseMoveEvent(QMouseEvent* event) {
    int hit = -1;
    for (int i = 0; i < counts_.size(); ++i) {
        const QRectF bar = barRect(i, 1);
        if (event->position().x() >= bar.left() - 3 && event->position().x() <= bar.right() + 3) hit = i;
    }
    if (hit != hovered_) {
        hovered_ = hit;
        update();
    }
}

void ActivityChart::leaveEvent(QEvent*) {
    hovered_ = -1;
    update();
}

// ====================================================================== RingChart

RingChart::RingChart(QWidget* parent) : QWidget(parent) {
    setFixedSize(176, 176);
    animation_.setDuration(1400);
    animation_.setEasingCurve(QEasingCurve::OutCubic);
    connect(&animation_, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
        value_ = v.toReal();
        update();
    });
    connect(&Theme::instance(), &Theme::changed, this, qOverload<>(&QWidget::update));
}

void RingChart::setRatio(qreal ratio) {
    animation_.stop();
    animation_.setStartValue(0.0);
    animation_.setEndValue(ratio);
    animation_.start();
}

void RingChart::paintEvent(QPaintEvent*) {
    const auto& pal = Theme::instance().palette();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF ring = QRectF(rect()).adjusted(12, 12, -12, -12);

    p.setPen(QPen(pal.surfaceHover, 14, Qt::SolidLine, Qt::RoundCap));
    p.drawEllipse(ring);
    if (value_ > 0.001) {
        QConicalGradient g(ring.center(), 90);
        g.setColorAt(0, QColor("#7fd6b9"));
        g.setColorAt(1 - value_, pal.accent);
        g.setColorAt(1, QColor("#7fd6b9"));
        p.setPen(QPen(QBrush(g), 14, Qt::SolidLine, Qt::RoundCap));
        p.drawArc(ring, 90 * 16, -int(value_ * 360 * 16));
    }

    p.setPen(pal.text);
    p.setFont(withSize(font(), 26, QFont::DemiBold));
    p.drawText(QRectF(0, height() / 2.0 - 30, width(), 36), Qt::AlignCenter, QStringLiteral("%1%").arg(qRound(value_ * 100)));
    p.setPen(pal.text3);
    p.setFont(withSize(font(), 12));
    p.drawText(QRectF(0, height() / 2.0 + 8, width(), 20), Qt::AlignCenter, tr("已借出"));
}

// ====================================================================== Sparkline

Sparkline::Sparkline(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_TransparentForMouseEvents);
    animation_.setDuration(1200);
    animation_.setStartValue(0.0);
    animation_.setEndValue(1.0);
    animation_.setEasingCurve(QEasingCurve::OutCubic);
    QObject::connect(&animation_, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
        progress_ = v.toReal();
        update();
    });
}

void Sparkline::setValues(const QList<int>& values) {
    values_ = values;
    animation_.stop();
    animation_.start();
}

void Sparkline::paintEvent(QPaintEvent*) {
    if (values_.size() < 2) return;
    const auto& pal = Theme::instance().palette();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const qreal w = width(), h = height();
    const int max = std::max(1, *std::max_element(values_.begin(), values_.end()));
    const qreal step = w / (values_.size() - 1);
    const auto point = [&](int i) { return QPointF(i * step, h - 3 - (h - 10) * values_[i] / max * progress_); };

    QPainterPath line(point(0));
    for (int i = 1; i < values_.size(); ++i) {
        const QPointF a = point(i - 1), b = point(i);
        const qreal cx = (a.x() + b.x()) / 2;
        line.cubicTo(QPointF(cx, a.y()), QPointF(cx, b.y()), b);
    }
    QPainterPath area = line;
    area.lineTo(w, h);
    area.lineTo(0, h);
    area.closeSubpath();

    QLinearGradient fill(0, 0, 0, h);
    QColor top = pal.accent;
    top.setAlpha(70);
    QColor bottom = pal.accent;
    bottom.setAlpha(0);
    fill.setColorAt(0, top);
    fill.setColorAt(1, bottom);
    p.fillPath(area, fill);
    QColor stroke = pal.accent;
    stroke.setAlpha(200);
    p.setPen(QPen(stroke, 1.6));
    p.drawPath(line);
}

// ====================================================================== SlidingIndicator

SlidingIndicator::SlidingIndicator(QWidget* parent) : QWidget(parent), animation_(this, "geometry") {
    setAttribute(Qt::WA_TransparentForMouseEvents);
    animation_.setDuration(460);
    animation_.setEasingCurve(QEasingCurve(QEasingCurve::OutBack));
    connect(&Theme::instance(), &Theme::changed, this, qOverload<>(&QWidget::update));
    lower();
}

void SlidingIndicator::moveTo(const QRect& target, bool animate) {
    animation_.stop();
    if (!animate || !isVisible()) {
        setGeometry(target);
        show();
        return;
    }
    animation_.setStartValue(geometry());
    animation_.setEndValue(target);
    animation_.start();
}

void SlidingIndicator::paintEvent(QPaintEvent*) {
    const auto& pal = Theme::instance().palette();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath pill;
    pill.addRoundedRect(QRectF(rect()), 8, 8);
    p.fillPath(pill, pal.accentSoft);
    QPainterPath bar;
    bar.addRoundedRect(QRectF(0, 10, 3, height() - 20), 1.5, 1.5);
    p.fillPath(bar, pal.accent);
}

}  // namespace ui
