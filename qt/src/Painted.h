#pragma once

#include <QColor>
#include <QList>
#include <QPropertyAnimation>
#include <QString>
#include <QVariantAnimation>
#include <QWidget>

class QPainter;
class QTimer;

/// Custom-painted widgets: everything that stylesheets cannot express
/// (gradients with motion, charts, generated covers). All read colours from
/// ui::Theme at paint time, so they follow light/dark switches automatically.
namespace ui {

/// Stable hue (0–359) from text — same algorithm as the web UI.
int hueOf(const QString& text);
/// First visible character, upper-cased.
QString initialOf(const QString& text);

/// Paints a book cover (coloured spine + initial) into `rect`.
void paintCover(QPainter& painter, const QRectF& rect, const QString& title);
/// Paints a round avatar with the last two characters of `name`.
void paintAvatar(QPainter& painter, const QRectF& rect, const QString& name);

/// Dashboard banner: deep green gradient, slowly drifting aurora blobs,
/// dotted texture and a little bookshelf that grows in on show.
class HeroBanner : public QWidget {
    Q_OBJECT
public:
    explicit HeroBanner(QWidget* parent = nullptr);
    void setText(const QString& eyebrow, const QString& title, const QString& subtitle);
    /// Places `action` on the right edge (reparented).
    void setAction(QWidget* action);
    void replayIntro();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    QString eyebrow_, title_, subtitle_;
    QWidget* action_ = nullptr;
    QTimer* tick_;
    qreal phase_ = 0;     ///< Aurora drift, advances continuously.
    qreal intro_ = 0;     ///< 0→1 bookshelf growth.
    QVariantAnimation introAnimation_;
};

/// Bar chart of daily counts. Bars grow in with a stagger; hovering a bar
/// shows a tooltip bubble. The last bar (today) is highlighted in amber.
class ActivityChart : public QWidget {
    Q_OBJECT
public:
    explicit ActivityChart(QWidget* parent = nullptr);
    void setData(const QList<int>& counts, const QStringList& labels);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    [[nodiscard]] QSize sizeHint() const override { return {480, 190}; }

private:
    [[nodiscard]] QRectF barRect(int index, qreal progress) const;

    QList<int> counts_;
    QStringList labels_;
    int hovered_ = -1;
    qreal progress_ = 1;
    QVariantAnimation animation_;
};

/// Animated donut: share of copies lent out.
class RingChart : public QWidget {
    Q_OBJECT
public:
    explicit RingChart(QWidget* parent = nullptr);
    void setRatio(qreal ratio);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    qreal value_ = 0;
    QVariantAnimation animation_;
};

/// Small smooth area chart drawn along the bottom of a stat card.
class Sparkline : public QWidget {
public:
    explicit Sparkline(QWidget* parent = nullptr);
    void setValues(const QList<int>& values);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QList<int> values_;
    qreal progress_ = 1;
    QVariantAnimation animation_;
};

/// Sidebar pill that slides (with a springy curve) to the active nav button.
class SlidingIndicator : public QWidget {
public:
    explicit SlidingIndicator(QWidget* parent);
    void moveTo(const QRect& target, bool animate = true);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QPropertyAnimation animation_;
};

}  // namespace ui
