#pragma once

#include <QList>
#include <QMainWindow>

class QButtonGroup;
class QLabel;
class QStackedWidget;
class QToolButton;
class QVBoxLayout;

namespace ui {

class BooksPage;
class LibraryController;
class LoansPage;
class SlidingIndicator;
class Toast;

/// Sidebar navigation (with sliding indicator) + stacked pages + toast overlay.
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(LibraryController& controller, QWidget* parent = nullptr);

    /// Grabs each page (light and dark) into `dir`. Used by --screenshot.
    void saveScreenshots(const QString& dir);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    enum PageIndex { Dashboard, Books, Students, Loans };

    QWidget* buildSidebar();
    void addNavButton(const QString& text, const QString& icon, PageIndex page, const QKeySequence& shortcut);
    void showPage(PageIndex page, bool animate = true);
    void updateOverdueBadge();

    LibraryController& controller_;
    QStackedWidget* stack_;
    QButtonGroup* nav_;
    QList<QToolButton*> navButtons_;
    QVBoxLayout* navLayout_ = nullptr;
    SlidingIndicator* indicator_ = nullptr;
    QLabel* overdueBadge_ = nullptr;
    BooksPage* booksPage_;
    LoansPage* loansPage_;
    Toast* toast_;
};

}  // namespace ui
