#pragma once

#include <QEasingCurve>
#include <QObject>
#include <QPointer>

class QLabel;
class QStackedWidget;
class QWidget;

/// Reusable motion primitives for the Qt UI. All durations follow the web
/// version (fast in, gentle out) so both front-ends feel the same.
namespace ui::motion {

inline constexpr int kFast = 180;
inline constexpr int kMedium = 320;
inline constexpr int kSlow = 700;

/// Fades a widget in after `delayMs` (use increasing delays for a stagger).
/// The opacity effect is removed afterwards so text renders crisply again.
void fadeIn(QWidget* widget, int delayMs = 0, int duration = 420);

/// Switches a QStackedWidget to `index`, fading a snapshot of the old page out on top.
void crossFade(QStackedWidget* stack, int index);

/// Counts a label from 0 to `value`, formatting with `format` ("%1" placeholder).
void countUp(QLabel* label, int value, const QString& format = QStringLiteral("%1"), int duration = 1000);

/// Freezes the current window contents and fades the snapshot away — used to
/// make theme switches cross-fade instead of flashing.
void snapshotFade(QWidget* window, int duration = 380);

/// Short horizontal shake (for validation errors).
void shake(QWidget* widget);

/// Applies a soft drop shadow (tinted, theme-aware strength).
void applyShadow(QWidget* widget, int blur = 28, int offsetY = 6, int alpha = 28);

}  // namespace ui::motion
