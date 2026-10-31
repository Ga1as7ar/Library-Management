#include "Dialogs.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpressionValidator>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QVBoxLayout>
#include <cmath>

#include "Animations.h"
#include "Common.h"
#include "LibraryController.h"
#include "Theme.h"
#include "Widgets.h"

namespace ui {

using library::Book;
using library::BookCategory;
using library::LibraryService;
using library::Student;

// ================================================================ FormDialog

FormDialog::FormDialog(const QString& title, const QString& submitLabel, QWidget* parent, const QString& icon)
    : QDialog(parent) {
    setWindowTitle(title);
    setModal(true);
    setMinimumWidth(520);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto* body = new QWidget(this);
    body->setObjectName("DialogBody");
    auto* bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(24, 22, 24, 22);
    bodyLayout->setSpacing(18);
    auto* head = new QHBoxLayout;
    head->setSpacing(12);
    auto* badge = new QLabel(body);
    badge->setObjectName("DialogIcon");
    badge->setFixedSize(38, 38);
    badge->setAlignment(Qt::AlignCenter);
    Theme::instance().bindIcon(badge, icon, 18, [](const Palette& p) { return p.accent; });
    auto* heading = makeLabel(title, "h2");
    heading->setStyleSheet(QStringLiteral("font-size: 17px;"));
    head->addWidget(badge);
    head->addWidget(heading);
    head->addStretch();
    bodyLayout->addLayout(head);

    grid_ = new QGridLayout;
    grid_->setHorizontalSpacing(16);
    grid_->setVerticalSpacing(14);
    grid_->setColumnStretch(0, 1);
    grid_->setColumnStretch(1, 1);
    bodyLayout->addLayout(grid_);

    error_ = new QLabel(body);
    error_->setObjectName("FormError");
    error_->setWordWrap(true);
    error_->hide();
    bodyLayout->addWidget(error_);
    outer->addWidget(body, 1);

    auto* footer = new QFrame(this);
    footer->setObjectName("DialogFooter");
    auto* footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(24, 14, 24, 14);
    footerLayout->addStretch();
    auto* cancel = makeButton(tr("取消"));
    submitButton_ = makeButton(submitLabel, {}, "primary");
    submitButton_->setDefault(true);
    footerLayout->addWidget(cancel);
    footerLayout->addWidget(submitButton_);
    outer->addWidget(footer);

    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(submitButton_, &QPushButton::clicked, this, &FormDialog::onSubmit);
}

void FormDialog::addField(const QString& label, QWidget* editor, bool required, const QString& hint, bool span) {
    if (span && nextColumn_ != 0) {
        ++nextRow_;
        nextColumn_ = 0;
    }
    auto* box = new QVBoxLayout;
    box->setSpacing(6);
    const QString text = required ? QStringLiteral("%1 <span style='color:%2'>*</span>")
                                        .arg(label.toHtmlEscaped(), Theme::instance().palette().danger.name())
                                  : label.toHtmlEscaped();
    auto* labelWidget = makeLabel(text, "fieldLabel");
    labelWidget->setTextFormat(Qt::RichText);
    labelWidget->setBuddy(editor);
    box->addWidget(labelWidget);
    box->addWidget(editor);
    if (!hint.isEmpty()) box->addWidget(makeLabel(hint, "hint"));
    box->addStretch();

    grid_->addLayout(box, nextRow_, nextColumn_, 1, span ? 2 : 1);
    if (span || nextColumn_ == 1) {
        ++nextRow_;
        nextColumn_ = 0;
    } else {
        nextColumn_ = 1;
    }
}

void FormDialog::onSubmit() {
    submitButton_->setEnabled(false);
    const auto failure = submit();
    submitButton_->setEnabled(true);
    if (failure) {
        error_->setText(*failure);
        const bool wasHidden = error_->isHidden();
        error_->show();
        if (wasHidden) adjustSize();
        motion::shake(error_);
        return;
    }
    accept();
}

// ================================================================ BookDialog

BookDialog::BookDialog(LibraryController& controller, std::optional<Book> book, QWidget* parent)
    : FormDialog(book ? tr("编辑图书") : tr("新增图书"), tr("保存"), parent, book ? "pencil" : "plus"),
      controller_(controller),
      book_(book.value_or(Book{})),
      editing_(book.has_value()) {
    title_ = new QLineEdit(qs(book_.title));
    title_->setMaxLength(200);
    author_ = new QLineEdit(qs(book_.author));
    publisher_ = new QLineEdit(qs(book_.publisher));
    isbn_ = new QLineEdit(qs(book_.isbn));
    isbn_->setValidator(new QRegularExpressionValidator(QRegularExpression("[0-9Xx\\- ]{0,17}"), isbn_));
    price_ = new QDoubleSpinBox;
    price_->setRange(0, 99999.99);
    price_->setDecimals(2);
    price_->setPrefix(QStringLiteral("¥ "));
    price_->setValue(book_.priceCents / 100.0);
    copies_ = new QSpinBox;
    copies_->setRange(0, 9999);
    copies_->setValue(book_.totalCopies);
    category_ = new QComboBox;
    category_->addItem(tr("普通图书"), QVariant::fromValue(int(BookCategory::General)));
    category_->addItem(tr("教材"), QVariant::fromValue(int(BookCategory::Textbook)));
    category_->setCurrentIndex(book_.category == BookCategory::Textbook ? 1 : 0);
    subject_ = new QLineEdit(qs(book_.subject));
    course_ = new QLineEdit(qs(book_.course));

    addField(tr("书名"), title_, true, {}, true);
    addField(tr("作者"), author_);
    addField(tr("出版社"), publisher_);
    addField(tr("ISBN"), isbn_, true, tr("10 或 13 位，可含连字符"));
    addField(tr("价格"), price_);
    addField(tr("馆藏数量"), copies_, true, editing_ ? tr("当前借出 %1 本").arg(book_.borrowedCopies()) : QString());
    addField(tr("类别"), category_);
    addField(tr("学科"), subject_);
    addField(tr("课程"), course_);

    const auto syncTextbookFields = [this] {
        const bool textbook = category_->currentIndex() == 1;
        subject_->setEnabled(textbook);
        course_->setEnabled(textbook);
        subject_->setPlaceholderText(textbook ? QString() : tr("仅教材"));
        course_->setPlaceholderText(textbook ? QString() : tr("仅教材"));
    };
    connect(category_, &QComboBox::currentIndexChanged, this, syncTextbookFields);
    syncTextbookFields();
}

std::optional<QString> BookDialog::submit() {
    Book draft = book_;
    draft.title = ss(title_->text());
    draft.author = ss(author_->text());
    draft.publisher = ss(publisher_->text());
    draft.isbn = ss(isbn_->text());
    draft.priceCents = std::llround(price_->value() * 100);
    draft.totalCopies = copies_->value();
    draft.category = static_cast<BookCategory>(category_->currentData().toInt());
    draft.subject = ss(subject_->text());
    draft.course = ss(course_->text());

    QString error;
    const auto saved = controller_.run(
        [&](LibraryService& s) { return editing_ ? s.updateBook(draft) : s.addBook(draft); }, &error);
    if (!saved) return error;
    book_ = *saved;
    return std::nullopt;
}

// ================================================================ StudentDialog

StudentDialog::StudentDialog(LibraryController& controller, std::optional<Student> student, QWidget* parent)
    : FormDialog(student ? tr("编辑读者") : tr("新增读者"), tr("保存"), parent, student ? "pencil" : "users"),
      controller_(controller),
      student_(student.value_or(Student{})),
      editing_(student.has_value()) {
    id_ = new QLineEdit(editing_ ? QString::number(student_.id) : QString());
    id_->setValidator(new QRegularExpressionValidator(QRegularExpression("[1-9][0-9]{0,17}"), id_));
    id_->setReadOnly(editing_);
    name_ = new QLineEdit(qs(student_.name));
    name_->setMaxLength(100);
    email_ = new QLineEdit(qs(student_.email));
    email_->setPlaceholderText(QStringLiteral("name@example.edu"));

    addField(tr("学号"), id_, true, editing_ ? tr("学号创建后不可修改") : QString(), true);
    addField(tr("姓名"), name_, true, {}, true);
    addField(tr("邮箱"), email_, false, tr("选填，用于到期提醒"), true);
    setMinimumWidth(420);
}

std::optional<QString> StudentDialog::submit() {
    Student draft = student_;
    draft.id = id_->text().toLongLong();
    draft.name = ss(name_->text());
    draft.email = ss(email_->text());

    QString error;
    const auto saved = controller_.run(
        [&](LibraryService& s) { return editing_ ? s.updateStudent(draft) : s.addStudent(draft); }, &error);
    if (!saved) return error;
    student_ = *saved;
    return std::nullopt;
}

// ================================================================ BorrowDialog

BorrowDialog::BorrowDialog(LibraryController& controller, std::optional<std::int64_t> studentId,
                           std::optional<std::int64_t> bookId, QWidget* parent)
    : FormDialog(tr("办理借阅"), tr("确认借出"), parent, "book-upload"), controller_(controller) {
    student_ = new QComboBox;
    book_ = new QComboBox;
    student_->setPlaceholderText(tr("请选择读者"));
    book_->setPlaceholderText(tr("请选择图书"));

    for (const auto& s : controller_.service().listStudents()) {
        student_->addItem(tr("%1（%2）· 在借 %3 本").arg(qs(s.name)).arg(s.id).arg(s.activeLoans),
                          QVariant::fromValue<qlonglong>(s.id));
    }
    for (const auto& b : controller_.service().listBooks()) {
        book_->addItem(tr("%1（可借 %2/%3）").arg(qs(b.title)).arg(b.availableCopies).arg(b.totalCopies),
                       QVariant::fromValue<qlonglong>(b.id));
        if (!b.canBorrow()) {
            // Grey out unavailable titles but keep them visible for context.
            if (auto* model = qobject_cast<QStandardItemModel*>(book_->model())) {
                model->item(book_->count() - 1)->setEnabled(false);
            }
        }
    }
    student_->setCurrentIndex(studentId ? student_->findData(QVariant::fromValue<qlonglong>(*studentId)) : -1);
    book_->setCurrentIndex(bookId ? book_->findData(QVariant::fromValue<qlonglong>(*bookId)) : -1);

    days_ = new QSpinBox;
    days_->setRange(1, 180);
    days_->setValue(LibraryService::kDefaultLoanDays);
    days_->setSuffix(tr(" 天"));

    addField(tr("读者"), student_, true, {}, true);
    addField(tr("图书"), book_, true, {}, true);
    addField(tr("借阅天数"), days_, true, tr("1–180 天，逾期未还将无法继续借书"), true);
}

std::optional<QString> BorrowDialog::submit() {
    if (student_->currentIndex() < 0) return tr("请选择读者");
    if (book_->currentIndex() < 0) return tr("请选择图书");

    QString error;
    const auto loan = controller_.run(
        [&](LibraryService& s) {
            return s.borrowBook(student_->currentData().toLongLong(), book_->currentData().toLongLong(), days_->value());
        },
        &error);
    if (!loan) return error;
    loan_ = *loan;
    return std::nullopt;
}

// ================================================================ confirm

bool confirm(QWidget* parent, const QString& title, const QString& message, const QString& confirmLabel,
             bool destructive) {
    QDialog dialog(parent);
    dialog.setWindowTitle(title);
    dialog.setMinimumWidth(420);

    auto* outer = new QVBoxLayout(&dialog);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto* body = new QWidget(&dialog);
    body->setObjectName("DialogBody");
    auto* row = new QHBoxLayout(body);
    row->setContentsMargins(24, 24, 24, 24);
    row->setSpacing(16);

    const auto& p = Theme::instance().palette();
    auto* icon = new QLabel(body);
    icon->setObjectName("ConfirmIcon");
    icon->setProperty("tone", destructive ? "danger" : "accent");
    icon->setFixedSize(44, 44);
    icon->setAlignment(Qt::AlignCenter);
    icon->setPixmap(Theme::instance()
                        .icon(destructive ? "alert-triangle" : "info-circle", destructive ? p.danger : p.accent, 20)
                        .pixmap(20, 20));
    row->addWidget(icon, 0, Qt::AlignTop);

    auto* texts = new QVBoxLayout;
    texts->setSpacing(4);
    texts->addWidget(makeLabel(title, "h2"));
    auto* text = makeLabel(message, "muted");
    text->setWordWrap(true);
    texts->addWidget(text);
    row->addLayout(texts, 1);
    outer->addWidget(body);

    auto* footer = new QFrame(&dialog);
    footer->setObjectName("DialogFooter");
    auto* buttons = new QHBoxLayout(footer);
    buttons->setContentsMargins(24, 14, 24, 14);
    buttons->addStretch();
    auto* cancel = makeButton(QObject::tr("取消"));
    auto* ok = makeButton(confirmLabel, {}, destructive ? "danger" : "primary");
    buttons->addWidget(cancel);
    buttons->addWidget(ok);
    outer->addWidget(footer);

    cancel->setDefault(true);
    cancel->setFocus();
    QObject::connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
    QObject::connect(ok, &QPushButton::clicked, &dialog, &QDialog::accept);
    return dialog.exec() == QDialog::Accepted;
}

}  // namespace ui
