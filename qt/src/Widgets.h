#pragma once

#include <QFrame>
#include <QList>
#include <QPushButton>
#include <QTableView>
#include <QToolButton>
#include <QVariantAnimation>

class QLabel;
class QLineEdit;
class QHBoxLayout;
class QVBoxLayout;
class QStandardItemModel;
class QSortFilterProxyModel;
class QTimer;
class QPropertyAnimation;
class QGraphicsDropShadowEffect;

namespace ui {

class Sparkline;

/// Button with a style variant ("primary", "ghost", "danger") and a theme-aware icon.
QPushButton* makeButton(const QString& text, const QString& iconName = {}, const char* variant = nullptr,
                        QWidget* parent = nullptr);

/// Label with a stylesheet role ("h1", "muted", ...).
QLabel* makeLabel(const QString& text, const char* role, QWidget* parent = nullptr);

/// Page title + subtitle on the left, an optional action widget on the right.
QHBoxLayout* makePageHeader(QLabel*& title, QLabel*& subtitle, QWidget* action);

/// Search field with a leading magnifier icon.
QLineEdit* makeSearchField(const QString& placeholder, QWidget* parent = nullptr);

/// Exclusive toggle group with a thumb that glides between options. Emits selected(index).
class SegmentedControl : public QFrame {
    Q_OBJECT
public:
    explicit SegmentedControl(const QStringList& labels, QWidget* parent = nullptr);
    void setCurrentIndex(int index);
    [[nodiscard]] int currentIndex() const { return current_; }
signals:
    void selected(int index);
protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
private:
    [[nodiscard]] QRectF thumbTarget() const;

    QList<QToolButton*> buttons_;
    int current_ = 0;
    QRectF thumb_;
    QVariantAnimation animation_;
};

/// Rounded surface with an optional header row (title, subtitle, action).
class Card : public QFrame {
public:
    explicit Card(const QString& title = {}, QWidget* headerAction = nullptr, QWidget* parent = nullptr,
                  const QString& subtitle = {});
    [[nodiscard]] QVBoxLayout* body() const { return body_; }
private:
    QVBoxLayout* body_;
};

/// Dashboard metric tile: label, counting-up value, caption, icon chip and an
/// optional sparkline. Lifts slightly on hover.
class StatCard : public QFrame {
public:
    StatCard(const QString& label, const QString& icon, QWidget* parent = nullptr);
    void setValue(int value, const QString& caption, bool alert = false);
    void setSpark(const QList<int>& values);
protected:
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
private:
    void refreshIcon();

    QLabel* value_;
    QLabel* caption_;
    QLabel* icon_;
    QString iconName_;
    Sparkline* spark_ = nullptr;
    bool alert_ = false;
    QGraphicsDropShadowEffect* shadow_;
    QVariantAnimation hover_;
};

/// Centered floating illustration + title + text, used for empty lists.
class EmptyState : public QWidget {
public:
    EmptyState(const QString& iconName, QWidget* parent = nullptr);
    void setText(const QString& title, const QString& text);
protected:
    void paintEvent(QPaintEvent* event) override;
private:
    QString iconName_;
    QLabel* title_;
    QLabel* text_;
    QTimer* float_;
    qreal phase_ = 0;
};

/// Read-only table preconfigured for the app look: row selection, tall rows,
/// sortable through a proxy, context menu + double-click forwarded as signals.
/// Shows an EmptyState overlay when the model has no rows.
class DataTable : public QTableView {
    Q_OBJECT
public:
    DataTable(QStandardItemModel* model, const QString& emptyIcon, QWidget* parent = nullptr);

    /// Source-model row of the current selection, or -1.
    [[nodiscard]] int currentSourceRow() const;
    void setEmptyText(const QString& title, const QString& text);
    /// Softly fades the rows in; call after repopulating the model.
    void revealRows();
    [[nodiscard]] QSortFilterProxyModel* proxy() const { return proxy_; }

signals:
    void rowActivated(int sourceRow);
    void contextMenuForRow(int sourceRow, const QPoint& globalPos);
    void currentRowChanged(int sourceRow);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void updateEmptyState();

    QStandardItemModel* model_;
    QSortFilterProxyModel* proxy_;
    EmptyState* empty_;
};

/// Transient notification: slides up from the bottom-right with a draining
/// progress line; stacks nothing, newer messages replace older ones.
class Toast : public QFrame {
public:
    explicit Toast(QWidget* parent);
    void show(const QString& message, bool success);
protected:
    void paintEvent(QPaintEvent* event) override;
private:
    void reposition(qreal lift);
    bool eventFilter(QObject* watched, QEvent* event) override;

    QLabel* icon_;
    QLabel* text_;
    QTimer* timer_;
    QVariantAnimation slide_;
    QVariantAnimation drain_;
    qreal lift_ = 0;
    qreal remaining_ = 1;
    bool success_ = true;
};

}  // namespace ui
