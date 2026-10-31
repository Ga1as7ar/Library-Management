#include "pages/StudentsPage.h"

#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QShortcut>
#include <QStandardItemModel>
#include <QTimer>

#include "Common.h"
#include "Delegates.h"
#include "Dialogs.h"
#include "Widgets.h"

namespace ui {

namespace {
enum Column { Name, Id, Loans, ColumnCount };
constexpr int kMaxLoans = library::LibraryService::kMaxActiveLoansPerStudent;
}  // namespace

StudentsPage::StudentsPage(LibraryController& controller, QWidget* parent) : Page(controller, parent) {
    auto* add = makeButton(tr("新增读者"), "plus", "primary");
    auto* title = makeLabel(tr("读者"), "h1");
    auto* subtitle = makeLabel(tr("每位读者最多同时借阅 %1 本").arg(kMaxLoans), "muted");
    layout_->addLayout(makePageHeader(title, subtitle, add));

    search_ = makeSearchField(tr("搜索姓名或学号"));
    loansButton_ = makeButton(tr("借阅记录"), "arrows-exchange", "ghost");
    borrowButton_ = makeButton(tr("借书"), "book-upload");
    editButton_ = makeButton(tr("编辑"), "pencil");
    deleteButton_ = makeButton(tr("删除"), "trash", "ghost");
    auto* toolbar = new QHBoxLayout;
    toolbar->setSpacing(12);
    toolbar->addWidget(search_);
    toolbar->addStretch();
    for (auto* button : {loansButton_, borrowButton_, editButton_, deleteButton_}) toolbar->addWidget(button);
    layout_->addLayout(toolbar);

    model_ = new QStandardItemModel(0, ColumnCount, this);
    model_->setHorizontalHeaderLabels({tr("读者"), tr("学号"), tr("借阅额度")});
    table_ = new DataTable(model_, "school");
    table_->setItemDelegateForColumn(Name, new TwoLineDelegate(table_));
    table_->setItemDelegateForColumn(Loans, new QuotaDelegate(table_));
    table_->horizontalHeader()->setSectionResizeMode(Name, QHeaderView::Stretch);
    table_->horizontalHeader()->resizeSection(Id, 200);
    table_->horizontalHeader()->resizeSection(Loans, 160);
    table_->sortByColumn(Id, Qt::AscendingOrder);
    layout_->addWidget(table_, 1);

    searchDelay_ = new QTimer(this);
    searchDelay_->setSingleShot(true);
    searchDelay_->setInterval(200);
    connect(search_, &QLineEdit::textChanged, searchDelay_, qOverload<>(&QTimer::start));
    connect(searchDelay_, &QTimer::timeout, this, &StudentsPage::reload);

    connect(add, &QPushButton::clicked, this, &StudentsPage::create);
    connect(loansButton_, &QPushButton::clicked, this, [this] { if (auto* s = selected()) emit showLoansRequested(s->id); });
    connect(borrowButton_, &QPushButton::clicked, this, [this] { if (auto* s = selected()) borrow(*s); });
    connect(editButton_, &QPushButton::clicked, this, [this] { if (auto* s = selected()) edit(*s); });
    connect(deleteButton_, &QPushButton::clicked, this, [this] { if (auto* s = selected()) remove(*s); });
    connect(table_, &DataTable::currentRowChanged, this, &StudentsPage::updateActions);
    connect(table_, &DataTable::rowActivated, this, [this](int row) { if (row >= 0) edit(students_[row]); });
    connect(table_, &DataTable::contextMenuForRow, this, [this](int row, const QPoint& pos) {
        const auto student = students_[row];
        QMenu menu;
        menu.addAction(tr("查看借阅记录"), this, [this, id = student.id] { emit showLoansRequested(id); });
        menu.addAction(tr("借书…"), this, [this, student] { borrow(student); })
            ->setEnabled(student.activeLoans < kMaxLoans);
        menu.addAction(tr("编辑…"), this, [this, student] { edit(student); });
        menu.addSeparator();
        menu.addAction(tr("删除"), this, [this, student] { remove(student); });
        menu.exec(pos);
    });

    // WidgetWithChildrenShortcut: only the visible page reacts, so pages don't clash.
    auto* newShortcut = new QShortcut(QKeySequence::New, this, this, &StudentsPage::create);
    auto* findShortcut = new QShortcut(QKeySequence::Find, this, search_, qOverload<>(&QWidget::setFocus));
    for (auto* shortcut : {newShortcut, findShortcut}) shortcut->setContext(Qt::WidgetWithChildrenShortcut);
    new QShortcut(QKeySequence::Delete, table_, this, [this] { if (auto* s = selected()) remove(*s); });
    updateActions();
}

void StudentsPage::reload() {
    const auto students = controller_.service().listStudents(ss(search_->text()));
    students_ = QList<library::Student>(students.begin(), students.end());

    model_->removeRows(0, model_->rowCount());
    for (const auto& student : students_) {
        auto* name = new QStandardItem(qs(student.name));
        name->setData(student.email.empty() ? tr("未填写邮箱") : qs(student.email), SubtitleRole);
        name->setData(qs(student.name), SortRole);
        name->setData(int(Leading::Avatar), LeadingRole);
        name->setData(search_->text().trimmed(), HighlightRole);
        auto* id = new QStandardItem(QString::number(student.id));
        id->setData(qlonglong(student.id), SortRole);
        id->setFont(QFont(QStringLiteral("Menlo")));
        auto* loans = new QStandardItem;
        loans->setData(student.activeLoans, SortRole);
        loans->setData(QPoint(student.activeLoans, kMaxLoans), StockRole);
        model_->appendRow({name, id, loans});
    }
    const bool filtered = !search_->text().isEmpty();
    table_->setEmptyText(filtered ? tr("没有匹配的读者") : tr("还没有读者"),
                         filtered ? tr("换个关键词试试") : tr("添加读者后即可办理借阅"));
    table_->revealRows();
    updateActions();
}

const library::Student* StudentsPage::selected() const {
    const int row = table_->currentSourceRow();
    return row >= 0 && row < students_.size() ? &students_[row] : nullptr;
}

void StudentsPage::updateActions() {
    const auto* student = selected();
    loansButton_->setEnabled(student);
    borrowButton_->setEnabled(student && student->activeLoans < kMaxLoans);
    editButton_->setEnabled(student);
    deleteButton_->setEnabled(student);
}

void StudentsPage::create() {
    StudentDialog dialog(controller_, std::nullopt, this);
    if (dialog.exec() == QDialog::Accepted) emit controller_.notify(tr("已添加读者 %1").arg(qs(dialog.result().name)));
}

void StudentsPage::edit(const library::Student& student) {
    StudentDialog dialog(controller_, student, this);
    if (dialog.exec() == QDialog::Accepted) emit controller_.notify(tr("已更新 %1").arg(qs(dialog.result().name)));
}

void StudentsPage::borrow(const library::Student& student) {
    BorrowDialog dialog(controller_, student.id, std::nullopt, this);
    if (dialog.exec() == QDialog::Accepted) {
        emit controller_.notify(tr("《%1》已借给 %2，应还日期 %3")
                                    .arg(qs(dialog.result().bookTitle), qs(dialog.result().studentName),
                                         formatDate(dialog.result().dueAt)));
    }
}

void StudentsPage::remove(const library::Student& student) {
    if (!confirm(this, tr("删除读者"),
                 tr("确定删除 %1（%2）？其历史借阅记录也会一并删除。").arg(qs(student.name)).arg(student.id),
                 tr("删除"))) {
        return;
    }
    QString error;
    const auto id = student.id;
    const auto name = qs(student.name);
    if (controller_.run([id](library::LibraryService& s) { s.removeStudent(id); return true; }, &error)) {
        emit controller_.notify(tr("已删除 %1").arg(name));
    } else {
        emit controller_.notify(error, false);
    }
}

}  // namespace ui
