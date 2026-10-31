#pragma once

#include <QDialog>
#include <functional>
#include <optional>

#include "library/Models.h"

class QComboBox;
class QDoubleSpinBox;
class QGridLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QWidget;

namespace ui {

class LibraryController;

/// Base for modal forms: title, a two-column field grid, inline error and a
/// footer with Cancel / Submit. Subclasses fill the grid and implement submit();
/// if submit() returns an error message the dialog stays open and shows it.
class FormDialog : public QDialog {
    Q_OBJECT

public:
    FormDialog(const QString& title, const QString& submitLabel, QWidget* parent, const QString& icon = QStringLiteral("pencil"));

protected:
    /// Adds a labelled field. `span` makes it take both columns.
    void addField(const QString& label, QWidget* editor, bool required = false, const QString& hint = {},
                  bool span = false);
    /// Returns an error message to keep the dialog open, or std::nullopt on success.
    virtual std::optional<QString> submit() = 0;

    QGridLayout* grid_;

private:
    void onSubmit();

    QLabel* error_;
    QPushButton* submitButton_;
    int nextRow_ = 0;
    int nextColumn_ = 0;
};

class BookDialog : public FormDialog {
    Q_OBJECT
public:
    /// Pass a book to edit it, or std::nullopt to create a new one.
    BookDialog(LibraryController& controller, std::optional<library::Book> book, QWidget* parent);
    [[nodiscard]] const library::Book& result() const { return book_; }

protected:
    std::optional<QString> submit() override;

private:
    LibraryController& controller_;
    library::Book book_;
    bool editing_;
    QLineEdit *title_, *author_, *publisher_, *isbn_, *subject_, *course_;
    QDoubleSpinBox* price_;
    QSpinBox* copies_;
    QComboBox* category_;
};

class StudentDialog : public FormDialog {
    Q_OBJECT
public:
    StudentDialog(LibraryController& controller, std::optional<library::Student> student, QWidget* parent);
    [[nodiscard]] const library::Student& result() const { return student_; }

protected:
    std::optional<QString> submit() override;

private:
    LibraryController& controller_;
    library::Student student_;
    bool editing_;
    QLineEdit *id_, *name_, *email_;
};

class BorrowDialog : public FormDialog {
    Q_OBJECT
public:
    BorrowDialog(LibraryController& controller, std::optional<std::int64_t> studentId,
                 std::optional<std::int64_t> bookId, QWidget* parent);
    [[nodiscard]] const library::Loan& result() const { return loan_; }

protected:
    std::optional<QString> submit() override;

private:
    LibraryController& controller_;
    library::Loan loan_;
    QComboBox *student_, *book_;
    QSpinBox* days_;
};

/// Yes/No confirmation with an icon. Returns true when confirmed.
bool confirm(QWidget* parent, const QString& title, const QString& message, const QString& confirmLabel,
             bool destructive = true);

}  // namespace ui
