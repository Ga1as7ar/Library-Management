#include "MainWindow.h"

#include <QApplication>
#include <QBoxLayout>
#include <QButtonGroup>
#include <QDateTime>
#include <QLabel>
#include <QShortcut>
#include <QStackedWidget>
#include <QTimer>
#include <QToolButton>

#include "Animations.h"
#include "LibraryController.h"
#include "Painted.h"
#include "Theme.h"
#include "Widgets.h"
#include "pages/BooksPage.h"
#include "pages/DashboardPage.h"
#include "pages/LoansPage.h"
#include "pages/StudentsPage.h"

namespace ui {

MainWindow::MainWindow(LibraryController& controller, QWidget* parent)
    : QMainWindow(parent), controller_(controller), stack_(new QStackedWidget), nav_(new QButtonGroup(this)) {
    setWindowTitle(tr("书阁 · 图书管理系统"));
    resize(1360, 880);
    setMinimumSize(1100, 680);

    auto* dashboard = new DashboardPage(controller_);
    booksPage_ = new BooksPage(controller_);
    auto* students = new StudentsPage(controller_);
    loansPage_ = new LoansPage(controller_);
    // Insertion order must match PageIndex.
    for (QWidget* page : {static_cast<QWidget*>(dashboard), static_cast<QWidget*>(booksPage_),
                          static_cast<QWidget*>(students), static_cast<QWidget*>(loansPage_)}) {
        stack_->addWidget(page);
    }

    auto* central = new QWidget;
    central->setObjectName("Content");
    auto* layout = new QHBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(buildSidebar());
    layout->addWidget(stack_, 1);
    setCentralWidget(central);

    toast_ = new Toast(central);
    connect(&controller_, &LibraryController::notify, toast_, &Toast::show);
    connect(&controller_, &LibraryController::dataChanged, this, &MainWindow::updateOverdueBadge);
    connect(dashboard, &DashboardPage::showAllLoansRequested, this, [this] {
        loansPage_->setStudentFilter(std::nullopt);
        showPage(Loans);
    });
    connect(dashboard, &DashboardPage::showBookRequested, this, [this](const QString& title) {
        booksPage_->setSearch(title);
        showPage(Books);
    });
    connect(students, &StudentsPage::showLoansRequested, this, [this](std::int64_t id) {
        loansPage_->setStudentFilter(id);
        showPage(Loans);
    });
    // Theme switch: freeze the old frame and fade it out over the new colours.
    connect(&Theme::instance(), &Theme::aboutToChange, this, [this] { motion::snapshotFade(this); });

    showPage(Dashboard, false);
    updateOverdueBadge();
}

QWidget* MainWindow::buildSidebar() {
    auto* sidebar = new QFrame;
    sidebar->setObjectName("Sidebar");
    sidebar->setFixedWidth(236);
    auto* layout = new QVBoxLayout(sidebar);
    layout->setContentsMargins(16, 24, 16, 18);
    layout->setSpacing(4);

    // Brand: gradient tile with the library glyph.
    auto* brand = new QHBoxLayout;
    brand->setSpacing(12);
    auto* mark = new QLabel;
    mark->setObjectName("BrandMark");
    mark->setFixedSize(38, 38);
    mark->setAlignment(Qt::AlignCenter);
    Theme::instance().bindIcon(mark, "library", 20, [](const Palette&) { return QColor(Qt::white); });
    motion::applyShadow(mark, 18, 5, 60);
    auto* names = new QVBoxLayout;
    names->setSpacing(0);
    auto* title = new QLabel(tr("书阁"));
    title->setObjectName("BrandTitle");
    auto* sub = new QLabel(QStringLiteral("LIBRARY · 图书管理"));
    sub->setObjectName("BrandSub");
    names->addWidget(title);
    names->addWidget(sub);
    brand->addWidget(mark);
    brand->addLayout(names);
    brand->addStretch();
    layout->addLayout(brand);
    layout->addSpacing(26);

    auto* section = new QLabel(tr("导航"));
    section->setObjectName("NavSection");
    layout->addWidget(section);
    layout->addSpacing(4);

    // Nav buttons live in their own container so the indicator can be positioned in it.
    auto* navHost = new QWidget;
    navLayout_ = new QVBoxLayout(navHost);
    navLayout_->setContentsMargins(0, 0, 0, 0);
    navLayout_->setSpacing(2);
    indicator_ = new SlidingIndicator(navHost);
    addNavButton(tr("概览"), "layout-dashboard", Dashboard, QKeySequence(Qt::CTRL | Qt::Key_1));
    addNavButton(tr("图书"), "books", Books, QKeySequence(Qt::CTRL | Qt::Key_2));
    addNavButton(tr("读者"), "users", Students, QKeySequence(Qt::CTRL | Qt::Key_3));
    addNavButton(tr("借阅"), "arrows-exchange", Loans, QKeySequence(Qt::CTRL | Qt::Key_4));
    layout->addWidget(navHost);

    // Red counter on the loans button.
    overdueBadge_ = new QLabel(navButtons_[Loans]);
    overdueBadge_->setObjectName("NavBadge");
    overdueBadge_->setAlignment(Qt::AlignCenter);
    overdueBadge_->setAttribute(Qt::WA_TransparentForMouseEvents);
    overdueBadge_->hide();

    layout->addStretch();

    auto* hint = new QLabel(tr("Ctrl+1–4 切换页面 · Ctrl+F 搜索"));
    hint->setObjectName("SidebarHint");
    hint->setWordWrap(true);
    layout->addWidget(hint);
    layout->addSpacing(6);

    auto* themeToggle = new QToolButton;
    themeToggle->setObjectName("ThemeToggle");
    themeToggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    themeToggle->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    themeToggle->setCursor(Qt::PointingHandCursor);
    themeToggle->setIconSize({18, 18});
    auto syncToggle = [themeToggle] {
        auto& theme = Theme::instance();
        const bool dark = theme.mode() == Theme::Mode::Dark;
        themeToggle->setText(dark ? tr("浅色模式") : tr("深色模式"));
        themeToggle->setIcon(theme.icon(dark ? "sun" : "moon", theme.palette().text3));
    };
    syncToggle();
    connect(&Theme::instance(), &Theme::changed, themeToggle, syncToggle);
    connect(themeToggle, &QToolButton::clicked, this, [] { Theme::instance().toggle(); });
    layout->addWidget(themeToggle);
    return sidebar;
}

void MainWindow::addNavButton(const QString& text, const QString& icon, PageIndex page, const QKeySequence& shortcut) {
    auto* button = new QToolButton;
    button->setText(text);
    button->setProperty("nav", true);
    button->setCheckable(true);
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    button->setFixedHeight(42);
    button->setCursor(Qt::PointingHandCursor);
    button->setIconSize({18, 18});
    button->setToolTip(QStringLiteral("%1  (%2)").arg(text, shortcut.toString(QKeySequence::NativeText)));
    // Icon follows both theme and checked state.
    auto updateIcon = [button, icon] {
        const auto& p = Theme::instance().palette();
        button->setIcon(Theme::instance().icon(icon, button->isChecked() ? p.accent : p.text2));
    };
    updateIcon();
    connect(button, &QToolButton::toggled, button, updateIcon);
    connect(&Theme::instance(), &Theme::changed, button, updateIcon);
    connect(button, &QToolButton::clicked, this, [this, page] {
        if (page == Loans) loansPage_->setStudentFilter(std::nullopt);
        showPage(page);
    });
    new QShortcut(shortcut, this, this, [button] { button->click(); });

    nav_->addButton(button, page);
    navButtons_ << button;
    navLayout_->addWidget(button);
}

void MainWindow::showPage(PageIndex page, bool animate) {
    if (animate && isVisible()) {
        motion::crossFade(stack_, page);
    } else {
        stack_->setCurrentIndex(page);
    }
    nav_->button(page)->setChecked(true);
    // Defer so the button geometry is final on first show.
    QTimer::singleShot(0, this, [this, page, animate] { indicator_->moveTo(navButtons_[page]->geometry(), animate); });
}

void MainWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
    if (auto* checked = nav_->checkedButton()) indicator_->moveTo(checked->geometry(), false);
}

void MainWindow::updateOverdueBadge() {
    const int overdue = controller_.service().stats().overdueLoans;
    overdueBadge_->setText(QString::number(overdue));
    overdueBadge_->setVisible(overdue > 0);
    overdueBadge_->adjustSize();
    const int w = std::max(20, overdueBadge_->width());
    overdueBadge_->setFixedSize(w, 20);
    overdueBadge_->move(navButtons_[Loans]->width() - w - 10, (navButtons_[Loans]->height() - 20) / 2);
    QTimer::singleShot(0, this, [this] {
        overdueBadge_->move(navButtons_[Loans]->width() - overdueBadge_->width() - 10,
                            (navButtons_[Loans]->height() - 20) / 2);
    });
}

void MainWindow::saveScreenshots(const QString& dir) {
    const char* names[] = {"dashboard", "books", "students", "loans"};
    for (auto mode : {Theme::Mode::Light, Theme::Mode::Dark}) {
        Theme::instance().setMode(mode);
        const char* suffix = mode == Theme::Mode::Dark ? "dark" : "light";
        for (int page = Dashboard; page <= Loans; ++page) {
            showPage(static_cast<PageIndex>(page), false);
            // Let entrance animations settle before grabbing.
            const auto until = QDateTime::currentMSecsSinceEpoch() + 1600;
            while (QDateTime::currentMSecsSinceEpoch() < until) QApplication::processEvents(QEventLoop::AllEvents, 30);
            grab().save(QStringLiteral("%1/qt-%2-%3.png").arg(dir, names[page], suffix));
        }
    }
}

}  // namespace ui
