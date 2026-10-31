#pragma once

#include <QList>

#include "pages/Page.h"

class QCheckBox;
class QLineEdit;
class QPushButton;
class QStandardItemModel;
class QTimer;

namespace ui {

class DataTable;
class SegmentedControl;

class BooksPage : public Page {
    Q_OBJECT
public:
    BooksPage(LibraryController& controller, QWidget* parent = nullptr);
    void reload() override;
    /// Fills the search box (e.g. when jumping here from the dashboard).
    void setSearch(const QString& text);

private:
    [[nodiscard]] const library::Book* selected() const;
    void updateActions();
    void create();
    void edit(const library::Book& book);
    void remove(const library::Book& book);
    void borrow(const library::Book& book);

    QLineEdit* search_;
    SegmentedControl* category_;
    QCheckBox* onlyAvailable_;
    QTimer* searchDelay_;
    QStandardItemModel* model_;
    DataTable* table_;
    QPushButton *borrowButton_, *editButton_, *deleteButton_;
    QList<library::Book> books_;
};

}  // namespace ui
