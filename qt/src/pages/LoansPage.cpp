#include "pages/LoansPage.h"

#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QStandardItemModel>
#include <QToolButton>

#include "Common.h"
#include "Delegates.h"
#include "Dialogs.h"
#include "Theme.h"
#include "Widgets.h"

namespace ui {

namespace {

enum Column { BookCol, StudentCol, Period, Status, ColumnCount };

/// Segment order in the status filter.
constexpr library::LoanFilter kFilters[] = {library::LoanFilter::Active, library::LoanFilter::Overdue,
                                            library::LoanFilter::Returned, library::LoanFilter::All};

}  // namespace

LoansPage::LoansPage(LibraryController& controller, QWidget* parent) : Page(controller, parent) {
    auto* borrow = makeButton(tr("办理借阅"), "book-upload", "primary");
    auto* title = makeLabel(tr("借阅"), "h1");
    auto* subtitle = makeLabel(tr("借出、归还与逾期跟踪 · 双击在借记录即可归还"), "muted");
    layout_->addLayout(makePageHeader(title, subtitle, borrow));

    status_ = new SegmentedControl({tr("在借"), tr("逾期"), tr("已归还"), tr("全部")});

    // Removable chip shown while filtered to a single student.
    studentChip_ = new QFrame;
    studentChip_->setObjectName("FilterChip");
    auto* chipLayout = new QHBoxLayout(studentChip_);
    chipLayout->setContentsMargins(12, 4, 4, 4);
    studentChipText_ = new QLabel;
    auto* clearChip = new QToolButton;
    clearChip->setAutoRaise(true);
    clearChip->setCursor(Qt::PointingHandCursor);
    clearChip->setToolTip(tr("查看全部读者"));
    Theme::instance().bindIcon(clearChip, "x", [](const Palette& p) { return p.accent; });
    chipLayout->addWidget(studentChipText_);
    chipLayout->addWidget(clearChip);
    studentChip_->hide();

    returnButton_ = makeButton(tr("归还"), "book-download");
    auto* toolbar = new QHBoxLayout;
    toolbar->setSpacing(12);
    toolbar->addWidget(status_);
    toolbar->addWidget(studentChip_);
    toolbar->addStretch();
    toolbar->addWidget(returnButton_);
    layout_->addLayout(toolbar);

    model_ = new QStandardItemModel(0, ColumnCount, this);
    model_->setHorizontalHeaderLabels({tr("图书"), tr("读者"), tr("借阅期间"), tr("状态")});
    table_ = new DataTable(model_, "arrows-exchange");
    table_->setItemDelegateForColumn(BookCol, new TwoLineDelegate(table_));
    table_->setItemDelegateForColumn(StudentCol, new TwoLineDelegate(table_));
    table_->setItemDelegateForColumn(Status, new BadgeDelegate(table_));
    auto* header = table_->horizontalHeader();
    header->setSectionResizeMode(BookCol, QHeaderView::Stretch);
    header->resizeSection(StudentCol, 210);
    header->resizeSection(Period, 220);
    header->resizeSection(Status, 170);
    table_->setSortingEnabled(false);  // service order is meaningful: soonest due first
    layout_->addWidget(table_, 1);

    connect(status_, &SegmentedControl::selected, this, &LoansPage::reload);
    connect(clearChip, &QToolButton::clicked, this, [this] { setStudentFilter(std::nullopt); });
    connect(borrow, &QPushButton::clicked, this, [this] {
        BorrowDialog dialog(controller_, studentId_, std::nullopt, this);
        if (dialog.exec() == QDialog::Accepted) {
            emit controller_.notify(tr("《%1》已借给 %2，应还日期 %3")
                                        .arg(qs(dialog.result().bookTitle), qs(dialog.result().studentName),
                                             formatDate(dialog.result().dueAt)));
        }
    });
    connect(returnButton_, &QPushButton::clicked, this, [this] { if (auto* l = selected()) giveBack(*l); });
    connect(table_, &DataTable::currentRowChanged, this, &LoansPage::updateActions);
    connect(table_, &DataTable::rowActivated, this, [this](int row) {
        if (row >= 0 && loans_[row].isActive()) giveBack(loans_[row]);
    });
    connect(table_, &DataTable::contextMenuForRow, this, [this](int row, const QPoint& pos) {
        const auto loan = loans_[row];
        QMenu menu;
        menu.addAction(tr("归还"), this, [this, loan] { giveBack(loan); })->setEnabled(loan.isActive());
        menu.addAction(tr("只看 %1 的借阅").arg(qs(loan.studentName)), this,
                       [this, id = loan.studentId] { setStudentFilter(id); });
        menu.exec(pos);
    });
    updateActions();
}

void LoansPage::setStudentFilter(std::optional<std::int64_t> studentId) {
    studentId_ = studentId;
    if (studentId_) {
        status_->setCurrentIndex(3);  // a student's history is most useful unfiltered
        QString name = QString::number(*studentId_);
        try {
            name = qs(controller_.service().getStudent(*studentId_).name);
        } catch (const std::exception&) {
        }
        studentChipText_->setText(tr("读者：%1").arg(name));
    }
    studentChip_->setVisible(studentId_.has_value());
    reload();
}

void LoansPage::reload() {
    library::LoanQuery query;
    query.studentId = studentId_;
    query.filter = kFilters[status_->currentIndex()];
    const auto loans = controller_.service().listLoans(query);
    loans_ = QList<library::Loan>(loans.begin(), loans.end());

    const auto now = controller_.now();
    model_->removeRows(0, model_->rowCount());
    for (const auto& loan : loans_) {
        auto* book = new QStandardItem(qs(loan.bookTitle));
        book->setData(tr("借阅号 #%1").arg(loan.id), SubtitleRole);
        book->setData(int(Leading::Cover), LeadingRole);
        auto* student = new QStandardItem(qs(loan.studentName));
        student->setData(QString::number(loan.studentId), SubtitleRole);
        student->setData(int(Leading::Avatar), LeadingRole);
        auto* period = new QStandardItem(QStringLiteral("%1 → %2").arg(formatDate(loan.borrowedAt), formatDate(loan.dueAt)));
        period->setForeground(Theme::instance().palette().text2);

        auto* status = new QStandardItem;
        if (loan.returnedAt) {
            status->setText(tr("已归还 · %1").arg(formatDate(*loan.returnedAt)));
            status->setData(int(Tone::Neutral), ToneRole);
        } else {
            const int days = daysUntil(loan.dueAt, now);
            status->setText(days < 0 ? tr("逾期 %1 天").arg(-days) : tr("剩 %1 天").arg(days));
            status->setData(int(days < 0 ? Tone::Danger : days <= 3 ? Tone::Warning : Tone::Accent), ToneRole);
        }
        model_->appendRow({book, student, period, status});
    }

    const bool overdue = query.filter == library::LoanFilter::Overdue;
    table_->setEmptyText(overdue ? tr("没有逾期借阅") : tr("暂无借阅记录"),
                         overdue ? tr("所有在借图书都在期限内") : tr("通过「办理借阅」借出第一本书"));
    table_->revealRows();
    updateActions();
}

const library::Loan* LoansPage::selected() const {
    const int row = table_->currentSourceRow();
    return row >= 0 && row < loans_.size() ? &loans_[row] : nullptr;
}

void LoansPage::updateActions() {
    const auto* loan = selected();
    returnButton_->setEnabled(loan && loan->isActive());
}

void LoansPage::giveBack(const library::Loan& loan) {
    if (!confirm(this, tr("确认归还"), tr("%1 归还《%2》？").arg(qs(loan.studentName), qs(loan.bookTitle)),
                 tr("确认归还"), false)) {
        return;
    }
    QString error;
    const auto id = loan.id;
    const auto title = qs(loan.bookTitle);
    if (controller_.run([id](library::LibraryService& s) { return s.returnBook(id); }, &error)) {
        emit controller_.notify(tr("《%1》已归还").arg(title));
    } else {
        emit controller_.notify(error, false);
    }
}

}  // namespace ui
