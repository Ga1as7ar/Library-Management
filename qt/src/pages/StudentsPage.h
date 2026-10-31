#pragma once

#include <QList>

#include "pages/Page.h"

class QLineEdit;
class QPushButton;
class QStandardItemModel;
class QTimer;

namespace ui {

class DataTable;

class StudentsPage : public Page {
    Q_OBJECT
public:
    StudentsPage(LibraryController& controller, QWidget* parent = nullptr);
    void reload() override;

signals:
    /// Asks the main window to open the loans page filtered to this student.
    void showLoansRequested(std::int64_t studentId);

private:
    [[nodiscard]] const library::Student* selected() const;
    void updateActions();
    void create();
    void edit(const library::Student& student);
    void remove(const library::Student& student);
    void borrow(const library::Student& student);

    QLineEdit* search_;
    QTimer* searchDelay_;
    QStandardItemModel* model_;
    DataTable* table_;
    QPushButton *loansButton_, *borrowButton_, *editButton_, *deleteButton_;
    QList<library::Student> students_;
};

}  // namespace ui
