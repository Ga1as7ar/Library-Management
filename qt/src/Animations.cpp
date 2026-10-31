#include "Animations.h"

#include <QGraphicsDropShadowEffect>
#include <QGraphicsOpacityEffect>
#include <QLabel>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QSequentialAnimationGroup>
#include <QStackedWidget>
#include <QTimer>
#include <QVariantAnimation>

#include "Theme.h"

namespace ui::motion {

void fadeIn(QWidget* widget, int delayMs, int duration) {
    if (!widget) return;
    // Widgets that already carry an effect (e.g. a drop shadow) are left alone:
    // Qt allows only one QGraphicsEffect per widget.
    if (widget->graphicsEffect() && !qobject_cast<QGraphicsOpacityEffect*>(widget->graphicsEffect())) return;

    auto* effect = new QGraphicsOpacityEffect(widget);
    effect->setOpacity(0.0);
    widget->setGraphicsEffect(effect);

    auto* opacity = new QPropertyAnimation(effect, "opacity", widget);
    opacity->setDuration(duration);
    opacity->setStartValue(0.0);
    opacity->setEndValue(1.0);
    opacity->setEasingCurve(QEasingCurve::OutCubic);
    QPointer<QWidget> guard(widget);
    QObject::connect(opacity, &QAbstractAnimation::finished, widget, [guard] {
        if (guard) guard->setGraphicsEffect(nullptr);  // opacity effects disable sub-pixel text; drop it
    });

    if (delayMs > 0) {
        QTimer::singleShot(delayMs, opacity, [opacity] { opacity->start(QAbstractAnimation::DeleteWhenStopped); });
    } else {
        opacity->start(QAbstractAnimation::DeleteWhenStopped);
    }
}

void crossFade(QStackedWidget* stack, int index) {
    if (!stack || stack->currentIndex() == index) return;
    QWidget* host = stack->parentWidget() ? stack->parentWidget() : stack;

    // Snapshot of the outgoing page, laid over the stack while the new page slides in.
    auto* overlay = new QLabel(host);
    overlay->setAttribute(Qt::WA_TransparentForMouseEvents);
    overlay->setPixmap(stack->grab());
    overlay->setGeometry(stack->geometry());
    overlay->show();
    overlay->raise();

    stack->setCurrentIndex(index);

    auto* effect = new QGraphicsOpacityEffect(overlay);
    overlay->setGraphicsEffect(effect);
    auto* fade = new QPropertyAnimation(effect, "opacity", overlay);
    fade->setDuration(kFast + 40);
    fade->setStartValue(1.0);
    fade->setEndValue(0.0);
    fade->setEasingCurve(QEasingCurve::InQuad);
    QObject::connect(fade, &QAbstractAnimation::finished, overlay, &QObject::deleteLater);
    fade->start(QAbstractAnimation::DeleteWhenStopped);


}

void countUp(QLabel* label, int value, const QString& format, int duration) {
    if (!label) return;
    auto* animation = new QVariantAnimation(label);
    animation->setDuration(duration);
    animation->setStartValue(0);
    animation->setEndValue(value);
    animation->setEasingCurve(QEasingCurve::OutQuart);
    QObject::connect(animation, &QVariantAnimation::valueChanged, label,
                     [label, format](const QVariant& v) { label->setText(format.arg(v.toInt())); });
    label->setText(format.arg(0));
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}

void snapshotFade(QWidget* window, int duration) {
    if (!window || !window->isVisible()) return;
    auto* overlay = new QLabel(window);
    overlay->setAttribute(Qt::WA_TransparentForMouseEvents);
    overlay->setPixmap(window->grab());
    overlay->setGeometry(window->rect());
    overlay->show();
    overlay->raise();
    auto* effect = new QGraphicsOpacityEffect(overlay);
    overlay->setGraphicsEffect(effect);
    auto* fade = new QPropertyAnimation(effect, "opacity", overlay);
    fade->setDuration(duration);
    fade->setStartValue(1.0);
    fade->setEndValue(0.0);
    fade->setEasingCurve(QEasingCurve::InOutQuad);
    QObject::connect(fade, &QAbstractAnimation::finished, overlay, &QObject::deleteLater);
    fade->start(QAbstractAnimation::DeleteWhenStopped);
}

void shake(QWidget* widget) {
    if (!widget) return;
    const QPoint origin = widget->pos();
    auto* animation = new QPropertyAnimation(widget, "pos", widget);
    animation->setDuration(420);
    for (int i = 1; i < 6; ++i) {
        animation->setKeyValueAt(i / 6.0, origin + QPoint(i % 2 ? -6 : 6, 0));
    }
    animation->setStartValue(origin);
    animation->setEndValue(origin);
    animation->setEasingCurve(QEasingCurve::OutCubic);
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}

void applyShadow(QWidget* widget, int blur, int offsetY, int alpha) {
    auto* shadow = new QGraphicsDropShadowEffect(widget);
    shadow->setBlurRadius(blur);
    shadow->setOffset(0, offsetY);
    auto update = [shadow, alpha] {
        const bool dark = Theme::instance().mode() == Theme::Mode::Dark;
        shadow->setColor(dark ? QColor(0, 0, 0, alpha * 4) : QColor(18, 40, 33, alpha));
    };
    update();
    QObject::connect(&Theme::instance(), &Theme::changed, shadow, update);
    widget->setGraphicsEffect(shadow);
}

}  // namespace ui::motion
