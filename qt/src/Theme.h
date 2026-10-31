#pragma once

#include <QColor>
#include <QHash>
#include <QIcon>
#include <QObject>
#include <functional>

class QAbstractButton;
class QLabel;

namespace ui {

/// Design tokens. Same names and values as the web UI's CSS variables.
struct Palette {
    QColor bg, surface, surface2, surfaceHover, border, borderStrong;
    QColor text, text2, text3;
    QColor accent, accentHover, accentSoft, accentText;
    QColor danger, dangerSoft, warning, warningSoft;
};

/// Application-wide light/dark theme. Owns the stylesheet (resources/style.qss
/// with @token placeholders), the QPalette, and tinted SVG icons.
class Theme : public QObject {
    Q_OBJECT

public:
    enum class Mode { Light, Dark };
    using ColorPicker = std::function<QColor(const Palette&)>;

    static Theme& instance();

    [[nodiscard]] Mode mode() const { return mode_; }
    [[nodiscard]] const Palette& palette() const { return palette_; }
    void setMode(Mode mode);
    void toggle() { setMode(mode_ == Mode::Light ? Mode::Dark : Mode::Light); }

    /// Tabler icon from :/icons tinted with `color`.
    [[nodiscard]] QIcon icon(const QString& name, const QColor& color, int size = 18) const;

    /// Sets an icon on a button / label and re-tints it whenever the theme changes.
    void bindIcon(QAbstractButton* button, const QString& name, ColorPicker color = textSecondary);
    void bindIcon(QLabel* label, const QString& name, int size, ColorPicker color = textTertiary);

    static QColor textSecondary(const Palette& p) { return p.text2; }
    static QColor textTertiary(const Palette& p) { return p.text3; }

signals:
    /// Emitted just before colours change (used for the cross-fade snapshot).
    void aboutToChange();
    void changed();

private:
    Theme();
    void apply();

    Mode mode_ = Mode::Light;
    Palette palette_;
    mutable QHash<QString, QIcon> iconCache_;
};

}  // namespace ui
