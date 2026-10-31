#pragma once

#include <QList>
#include <optional>

#include "pages/Page.h"

class QFrame;
class QLabel;
class QPushButton;
class QStandardItemModel;

namespace ui {

class DataTable;
class SegmentedControl;

class LoansPage : public Page {
    Q_OBJECT
public:
    LoansPage(LibraryController& controller, QWidget* parent = nullptr);
    void reload() override;

    /// Restricts the list to one student (std::nullopt shows everyone).
    void setStudentFilter(std::optional<std::int64_t> studentId);

private:
    [[nodiscard]] const library::Loan* selected() const;
    void updateActions();
    void giveBack(const library::Loan& loan);

    SegmentedControl* status_;
    QFrame* studentChip_;
    QLabel* studentChipText_;
    QStandardItemModel* model_;
    DataTable* table_;
    QPushButton* returnButton_;
    std::optional<std::int64_t> studentId_;
    QList<library::Loan> loans_;
};

}  // namespace ui
