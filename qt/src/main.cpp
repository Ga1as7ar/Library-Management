#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QMessageBox>
#include <QStandardPaths>
#include <QTimer>

#include "LibraryController.h"
#include "MainWindow.h"
#include "Theme.h"

namespace {

/// Default database location: the per-user app-data directory,
/// e.g. ~/Library/Application Support/LibraryManagement/library.db on macOS.
QString defaultDatabasePath() {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return QDir(dir).filePath(QStringLiteral("library.db"));
}

}  // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("LibraryManagement"));
    QApplication::setApplicationName(QStringLiteral("LibraryManagement"));
    QApplication::setApplicationDisplayName(QObject::tr("书阁"));
    QApplication::setStyle(QStringLiteral("Fusion"));  // consistent base for the stylesheet on every OS

    QCommandLineParser parser;
    parser.setApplicationDescription(QObject::tr("书阁 · 图书管理系统（Qt 版）"));
    parser.addHelpOption();
    QCommandLineOption dbOption({"d", "db"}, QObject::tr("SQLite 数据库文件路径"), QObject::tr("path"));
    QCommandLineOption noSeedOption("no-seed", QObject::tr("空数据库时不写入示例数据"));
    QCommandLineOption darkOption("dark", QObject::tr("以深色主题启动"));
    QCommandLineOption screenshotOption("screenshot", QObject::tr("把各页面截图保存到目录后退出"), QObject::tr("dir"));
    parser.addOptions({dbOption, noSeedOption, darkOption, screenshotOption});
    parser.process(app);

    ui::Theme::instance();  // apply stylesheet before any widget is created
    if (parser.isSet(darkOption)) ui::Theme::instance().setMode(ui::Theme::Mode::Dark);

    const QString dbPath = parser.isSet(dbOption) ? parser.value(dbOption) : defaultDatabasePath();
    std::unique_ptr<ui::LibraryController> controller;
    try {
        auto database = std::make_shared<library::Database>(dbPath.toStdString());
        auto service = std::make_unique<library::LibraryService>(std::move(database));
        if (!parser.isSet(noSeedOption)) service->seedIfEmpty();
        controller = std::make_unique<ui::LibraryController>(std::move(service));
    } catch (const std::exception& error) {
        QMessageBox::critical(nullptr, QObject::tr("无法打开数据库"),
                              QObject::tr("%1\n\n%2").arg(dbPath, QString::fromUtf8(error.what())));
        return 1;
    }

    ui::MainWindow window(*controller);
    window.show();
    if (parser.isSet(screenshotOption)) {
        // Render every page to <dir>/qt-<n>.png, then quit (visual regression / docs).
        const QString dir = parser.value(screenshotOption);
        QDir().mkpath(dir);
        QTimer::singleShot(300, &window, [&window, dir] {
            window.saveScreenshots(dir);
            QApplication::quit();
        });
    }
    return QApplication::exec();
}
