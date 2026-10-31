#include "pages/BooksPage.h"

#include <QCheckBox>
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
#include "Theme.h"
#include "Widgets.h"

namespace ui {

namespace {
enum Column { Title, Category, Isbn, Price, Stock, ColumnCount };
}

BooksPage::BooksPage(LibraryController& controller, QWidget* parent) : Page(controller, parent) {
    auto* add = makeButton(tr("新增图书"), "plus", "primary");
    auto* title = makeLabel(tr("图书"), "h1");
    auto* subtitle = makeLabel(tr("管理馆藏目录与库存 · 双击编辑，右键更多操作"), "muted");
    layout_->addLayout(makePageHeader(title, subtitle, add));

    // Toolbar: filters on the left, row actions on the right.
    search_ = makeSearchField(tr("搜索书名、作者、ISBN、出版社"));
    category_ = new SegmentedControl({tr("全部"), tr("普通"), tr("教材")});
    onlyAvailable_ = new QCheckBox(tr("仅显示可借"));
    borrowButton_ = makeButton(tr("借出"), "book-upload");
    editButton_ = makeButton(tr("编辑"), "pencil");
    deleteButton_ = makeButton(tr("删除"), "trash", "ghost");

    auto* toolbar = new QHBoxLayout;
    toolbar->setSpacing(12);
    toolbar->addWidget(search_);
    toolbar->addWidget(category_);
    toolbar->addWidget(onlyAvailable_);
    toolbar->addStretch();
    toolbar->addWidget(borrowButton_);
    toolbar->addWidget(editButton_);
    toolbar->addWidget(deleteButton_);
    layout_->addLayout(toolbar);

    model_ = new QStandardItemModel(0, ColumnCount, this);
    model_->setHorizontalHeaderLabels({tr("书名"), tr("类别"), tr("ISBN"), tr("价格"), tr("库存")});
    table_ = new DataTable(model_, "books");
    table_->setItemDelegateForColumn(Title, new TwoLineDelegate(table_));
    table_->setItemDelegateForColumn(Category, new BadgeDelegate(table_));
    table_->setItemDelegateForColumn(Stock, new StockDelegate(table_));
    auto* header = table_->horizontalHeader();
    header->setSectionResizeMode(Title, QHeaderView::Stretch);
    header->resizeSection(Category, 90);
    header->resizeSection(Isbn, 150);
    header->resizeSection(Price, 100);
    header->resizeSection(Stock, 200);
    table_->setItemDelegateForColumn(Isbn, new RowDelegate(table_));
    table_->sortByColumn(Title, Qt::AscendingOrder);
    layout_->addWidget(table_, 1);

    // Debounce typing so each keystroke doesn't hit the database.
    searchDelay_ = new QTimer(this);
    searchDelay_->setSingleShot(true);
    searchDelay_->setInterval(200);
    connect(search_, &QLineEdit::textChanged, searchDelay_, qOverload<>(&QTimer::start));
    connect(searchDelay_, &QTimer::timeout, this, &BooksPage::reload);
    connect(category_, &SegmentedControl::selected, this, &BooksPage::reload);
    connect(onlyAvailable_, &QCheckBox::toggled, this, &BooksPage::reload);

    connect(add, &QPushButton::clicked, this, &BooksPage::create);
    connect(borrowButton_, &QPushButton::clicked, this, [this] { if (auto* b = selected()) borrow(*b); });
    connect(editButton_, &QPushButton::clicked, this, [this] { if (auto* b = selected()) edit(*b); });
    connect(deleteButton_, &QPushButton::clicked, this, [this] { if (auto* b = selected()) remove(*b); });
    connect(table_, &DataTable::currentRowChanged, this, &BooksPage::updateActions);
    connect(table_, &DataTable::rowActivated, this, [this](int row) { if (row >= 0) edit(books_[row]); });
    connect(table_, &DataTable::contextMenuForRow, this, [this](int row, const QPoint& pos) {
        const auto book = books_[row];
        QMenu menu;
        menu.addAction(tr("借出…"), this, [this, book] { borrow(book); })->setEnabled(book.canBorrow());
        menu.addAction(tr("编辑…"), this, [this, book] { edit(book); });
        menu.addSeparator();
        menu.addAction(tr("删除"), this, [this, book] { remove(book); });
        menu.exec(pos);
    });

    // WidgetWithChildrenShortcut: only the visible page reacts, so pages don't clash.
    auto* newShortcut = new QShortcut(QKeySequence::New, this, this, &BooksPage::create);
    auto* findShortcut = new QShortcut(QKeySequence::Find, this, search_, qOverload<>(&QWidget::setFocus));
    for (auto* shortcut : {newShortcut, findShortcut}) shortcut->setContext(Qt::WidgetWithChildrenShortcut);
    new QShortcut(QKeySequence::Delete, table_, this, [this] { if (auto* b = selected()) remove(*b); });
    updateActions();
}

void BooksPage::setSearch(const QString& text) {
    search_->setText(text);
    searchDelay_->stop();
    reload();
}

void BooksPage::reload() {
    library::BookQuery query;
    query.keyword = ss(search_->text());
    query.onlyAvailable = onlyAvailable_->isChecked();
    if (category_->currentIndex() == 1) query.category = library::BookCategory::General;
    if (category_->currentIndex() == 2) query.category = library::BookCategory::Textbook;

    const auto books = controller_.service().listBooks(query);
    books_ = QList<library::Book>(books.begin(), books.end());

    model_->removeRows(0, model_->rowCount());
    for (const auto& book : books_) {
        const bool textbook = book.category == library::BookCategory::Textbook;
        QStringList details{book.author.empty() ? tr("佚名") : qs(book.author)};
        if (!book.publisher.empty()) details << qs(book.publisher);
        if (textbook && !book.course.empty()) details << qs(book.course);

        auto* title = new QStandardItem(qs(book.title));
        title->setData(details.join(QStringLiteral(" · ")), SubtitleRole);
        title->setData(qs(book.title), SortRole);
        title->setToolTip(qs(book.title));
        title->setData(int(Leading::Cover), LeadingRole);
        title->setData(search_->text().trimmed(), HighlightRole);
        auto* category = new QStandardItem(textbook ? tr("教材") : tr("普通"));
        category->setData(int(textbook ? Tone::Accent : Tone::Neutral), ToneRole);
        category->setData(textbook ? 1 : 0, SortRole);
        auto* isbn = new QStandardItem(qs(book.isbn));
        isbn->setData(qs(book.isbn), SortRole);
        isbn->setFont(QFont(QStringLiteral("Menlo")));
        auto* price = new QStandardItem(formatPrice(book.priceCents));
        price->setData(qlonglong(book.priceCents), SortRole);
        price->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        auto* stock = new QStandardItem;
        stock->setData(QPoint(book.availableCopies, book.totalCopies), StockRole);
        stock->setData(book.availableCopies, SortRole);
        model_->appendRow({title, category, isbn, price, stock});
    }

    const bool filtered = !search_->text().isEmpty() || category_->currentIndex() || onlyAvailable_->isChecked();
    table_->setEmptyText(filtered ? tr("没有匹配的图书") : tr("馆藏为空"),
                         filtered ? tr("调整筛选条件试试") : tr("点击右上角「新增图书」开始建立馆藏"));
    table_->revealRows();
    updateActions();
}

const library::Book* BooksPage::selected() const {
    const int row = table_->currentSourceRow();
    return row >= 0 && row < books_.size() ? &books_[row] : nullptr;
}

void BooksPage::updateActions() {
    const auto* book = selected();
    borrowButton_->setEnabled(book && book->canBorrow());
    editButton_->setEnabled(book);
    deleteButton_->setEnabled(book);
}

void BooksPage::create() {
    BookDialog dialog(controller_, std::nullopt, this);
    if (dialog.exec() == QDialog::Accepted) emit controller_.notify(tr("已添加《%1》").arg(qs(dialog.result().title)));
}

void BooksPage::edit(const library::Book& book) {
    BookDialog dialog(controller_, book, this);
    if (dialog.exec() == QDialog::Accepted) emit controller_.notify(tr("已更新《%1》").arg(qs(dialog.result().title)));
}

void BooksPage::borrow(const library::Book& book) {
    BorrowDialog dialog(controller_, std::nullopt, book.id, this);
    if (dialog.exec() == QDialog::Accepted) {
        emit controller_.notify(tr("《%1》已借给 %2，应还日期 %3")
                                    .arg(qs(dialog.result().bookTitle), qs(dialog.result().studentName),
                                         formatDate(dialog.result().dueAt)));
    }
}

void BooksPage::remove(const library::Book& book) {
    if (!confirm(this, tr("删除图书"), tr("确定删除《%1》？该书的历史借阅记录也会一并删除。").arg(qs(book.title)),
                 tr("删除"))) {
        return;
    }
    QString error;
    const auto id = book.id;
    const auto name = qs(book.title);
    if (controller_.run([id](library::LibraryService& s) { s.removeBook(id); return true; }, &error)) {
        emit controller_.notify(tr("已删除《%1》").arg(name));
    } else {
        emit controller_.notify(error, false);
    }
}

}  // namespace ui
