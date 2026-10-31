#include "Theme.h"

#include <QAbstractButton>
#include <QApplication>
#include <QFile>
#include <QLabel>
#include <QPainter>
#include <QSettings>
#include <QSvgRenderer>
#include <algorithm>
#include <utility>
#include <vector>

namespace ui {

namespace {

Palette lightPalette() {
    return {
        .bg = QColor("#f4f5f6"), .surface = QColor("#ffffff"), .surface2 = QColor("#f8f9fa"),
        .surfaceHover = QColor("#eef0f1"), .border = QColor("#e3e6e8"), .borderStrong = QColor("#cdd2d6"),
        .text = QColor("#16191c"), .text2 = QColor("#4d555c"), .text3 = QColor("#6b737a"),
        .accent = QColor("#1f6f5c"), .accentHover = QColor("#185a4a"), .accentSoft = QColor("#e4f1ed"),
        .accentText = QColor("#ffffff"),
        .danger = QColor("#b4332b"), .dangerSoft = QColor("#fbeceb"),
        .warning = QColor("#9a5b00"), .warningSoft = QColor("#fdf3e1"),
    };
}

Palette darkPalette() {
    return {
        .bg = QColor("#0f1112"), .surface = QColor("#17191b"), .surface2 = QColor("#1c1f21"),
        .surfaceHover = QColor("#24282b"), .border = QColor("#2a2e31"), .borderStrong = QColor("#3a3f43"),
        .text = QColor("#eceeef"), .text2 = QColor("#b3b9be"), .text3 = QColor("#8b9399"),
        .accent = QColor("#4fb89b"), .accentHover = QColor("#67c8ac"), .accentSoft = QColor("#173029"),
        .accentText = QColor("#08130f"),
        .danger = QColor("#ef7a72"), .dangerSoft = QColor("#3a1c1a"),
        .warning = QColor("#e3a74a"), .warningSoft = QColor("#33270f"),
    };
}

/// Substitutes @tokens in the stylesheet template. Longest names first so that
/// "@surfaceHover" is not clobbered by "@surface".
QString renderStyleSheet(const Palette& p) {
    QFile file(QStringLiteral(":/style/style.qss"));
    if (!file.open(QIODevice::ReadOnly)) return {};
    QString qss = QString::fromUtf8(file.readAll());

    std::vector<std::pair<QString, QColor>> tokens = {
        {"bg", p.bg}, {"surface", p.surface}, {"surface2", p.surface2}, {"surfaceHover", p.surfaceHover},
        {"border", p.border}, {"borderStrong", p.borderStrong}, {"text", p.text}, {"text2", p.text2},
        {"text3", p.text3}, {"accent", p.accent}, {"accentHover", p.accentHover}, {"accentSoft", p.accentSoft},
        {"accentText", p.accentText}, {"danger", p.danger}, {"dangerSoft", p.dangerSoft}, {"warning", p.warning},
        {"warningSoft", p.warningSoft},
    };
    std::sort(tokens.begin(), tokens.end(), [](const auto& a, const auto& b) { return a.first.size() > b.first.size(); });
    for (const auto& [name, color] : tokens) qss.replace('@' + name, color.name());
    return qss;
}

QPalette toQPalette(const Palette& p) {
    QPalette pal;
    pal.setColor(QPalette::Window, p.bg);
    pal.setColor(QPalette::WindowText, p.text);
    pal.setColor(QPalette::Base, p.surface);
    pal.setColor(QPalette::AlternateBase, p.surface2);
    pal.setColor(QPalette::Text, p.text);
    pal.setColor(QPalette::Button, p.surface);
    pal.setColor(QPalette::ButtonText, p.text);
    pal.setColor(QPalette::Highlight, p.accent);
    pal.setColor(QPalette::HighlightedText, p.accentText);
    pal.setColor(QPalette::PlaceholderText, p.text3);
    pal.setColor(QPalette::ToolTipBase, p.surface);
    pal.setColor(QPalette::ToolTipText, p.text);
    pal.setColor(QPalette::Mid, p.border);
    for (auto role : {QPalette::Text, QPalette::ButtonText, QPalette::WindowText}) {
        pal.setColor(QPalette::Disabled, role, p.text3);
    }
    return pal;
}

}  // namespace

Theme& Theme::instance() {
    static Theme theme;
    return theme;
}

Theme::Theme() {
    const QString saved = QSettings().value(QStringLiteral("theme"), QStringLiteral("light")).toString();
    mode_ = saved == QLatin1String("dark") ? Mode::Dark : Mode::Light;
    apply();
}

void Theme::setMode(Mode mode) {
    if (mode == mode_) return;
    emit aboutToChange();
    mode_ = mode;
    QSettings().setValue(QStringLiteral("theme"), mode_ == Mode::Dark ? "dark" : "light");
    apply();
}

void Theme::apply() {
    palette_ = mode_ == Mode::Dark ? darkPalette() : lightPalette();
    iconCache_.clear();
    QApplication::setPalette(toQPalette(palette_));
    qApp->setStyleSheet(renderStyleSheet(palette_));
    emit changed();
}

QIcon Theme::icon(const QString& name, const QColor& color, int size) const {
    const QString key = QStringLiteral("%1|%2|%3").arg(name, color.name(QColor::HexArgb)).arg(size);
    if (auto it = iconCache_.constFind(key); it != iconCache_.constEnd()) return *it;

    QFile file(QStringLiteral(":/icons/%1.svg").arg(name));
    if (!file.open(QIODevice::ReadOnly)) return {};
    QByteArray svg = file.readAll();
    svg.replace("currentColor", color.name().toLatin1());
    svg.replace("stroke-width=\"2\"", "stroke-width=\"1.75\"");

    QSvgRenderer renderer(svg);
    const qreal ratio = std::max<qreal>(2.0, qApp->devicePixelRatio());
    QPixmap pixmap(QSize(size, size) * ratio);
    pixmap.fill(Qt::transparent);
    pixmap.setDevicePixelRatio(ratio);
    {
        QPainter painter(&pixmap);
        renderer.render(&painter, QRectF(0, 0, size, size));
    }
    QIcon icon(pixmap);
    icon.addPixmap(pixmap, QIcon::Disabled);  // keep color; disabled look comes from button opacity
    iconCache_.insert(key, icon);
    return icon;
}

void Theme::bindIcon(QAbstractButton* button, const QString& name, ColorPicker color) {
    auto update = [this, button, name, color = std::move(color)] { button->setIcon(icon(name, color(palette_))); };
    update();
    connect(this, &Theme::changed, button, update);
}

void Theme::bindIcon(QLabel* label, const QString& name, int size, ColorPicker color) {
    auto update = [this, label, name, size, color = std::move(color)] {
        label->setPixmap(icon(name, color(palette_), size).pixmap(size, size));
    };
    update();
    connect(this, &Theme::changed, label, update);
}

}  // namespace ui
