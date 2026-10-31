#include "pages/DashboardPage.h"

#include <QDate>
#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScrollArea>
#include <QStandardItemModel>
#include <QTimeZone>

#include "Animations.h"
#include "Common.h"
#include "Delegates.h"
#include "Dialogs.h"
#include "Painted.h"
#include "Theme.h"
#include "Widgets.h"

namespace ui {

namespace {

constexpr int kDays = 14;

QString greeting() {
    const int hour = QTime::currentTime().hour();
    if (hour < 6) return QObject::tr("夜深了");
    if (hour < 11) return QObject::tr("早上好");
    if (hour < 14) return QObject::tr("中午好");
    if (hour < 18) return QObject::tr("下午好");
    return QObject::tr("晚上好");
}

/// One row of the "热门图书" list: rank, painted cover, title/author, availability chip.
class RankRow : public QWidget {
public:
    RankRow(int rank, const library::Book& book, QWidget* parent) : QWidget(parent), rank_(rank), book_(book) {
        setFixedHeight(62);
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover);
    }
    std::function<void()> onClick;

protected:
    void paintEvent(QPaintEvent*) override {
        const auto& p = Theme::instance().palette();
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const bool hovered = underMouse();
        if (hovered) painter.fillRect(rect(), p.surface2);
        if (rank_ > 1) {
            painter.setPen(p.border);
            painter.drawLine(QPointF(22, 0), QPointF(width() - 22, 0));
        }

        QFont f = font();
        f.setWeight(QFont::DemiBold);
        painter.setFont(f);
        painter.setPen(rank_ == 1 ? QColor("#c98a2b") : p.text3);
        painter.drawText(QRect(18, 0, 20, height()), Qt::AlignCenter, QString::number(rank_));

        QRectF cover(46, (height() - 42) / 2.0, 30, 42);
        if (hovered) {
            painter.save();
            painter.translate(cover.center());
            painter.rotate(-6);
            painter.translate(-cover.center());
        }
        paintCover(painter, cover, qs(book_.title));
        if (hovered) painter.restore();

        const QString chip = tr("可借 %1").arg(book_.availableCopies);
        QFont small = font();
        small.setPointSizeF(font().pointSizeF() - 1);
        small.setWeight(QFont::Medium);
        const int chipW = QFontMetrics(small).horizontalAdvance(chip) + 18;
        const QRectF chipRect(width() - 22 - chipW, height() / 2.0 - 11, chipW, 22);

        const int textLeft = 90;
        const int textW = int(chipRect.left()) - textLeft - 10;
        painter.setFont(f);
        painter.setPen(p.text);
        painter.drawText(QRect(textLeft, 12, textW, 20), Qt::AlignLeft | Qt::AlignVCenter,
                         QFontMetrics(f).elidedText(qs(book_.title), Qt::ElideRight, textW));
        QFont sub = font();
        sub.setPointSizeF(font().pointSizeF() - 1.5);
        painter.setFont(sub);
        painter.setPen(p.text3);
        painter.drawText(QRect(textLeft, 32, textW, 18), Qt::AlignLeft | Qt::AlignVCenter,
                         book_.author.empty() ? tr("佚名") : qs(book_.author));

        QPainterPath pill;
        pill.addRoundedRect(chipRect, 11, 11);
        painter.fillPath(pill, book_.availableCopies ? p.accentSoft : p.surfaceHover);
        painter.setFont(small);
        painter.setPen(book_.availableCopies ? p.accent : p.text3);
        painter.drawText(chipRect, Qt::AlignCenter, chip);
    }
    void mouseReleaseEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && onClick) onClick();
    }

private:
    int rank_;
    library::Book book_;
};

}  // namespace

DashboardPage::DashboardPage(LibraryController& controller, QWidget* parent) : Page(controller, parent) {
    // The dashboard is taller than small windows: wrap content in a scroll area.
    layout_->setContentsMargins(0, 0, 0, 0);
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setObjectName("PageScroll");
    auto* content = new QWidget;
    content->setObjectName("PageContent");
    auto* root = new QVBoxLayout(content);
    root->setContentsMargins(32, 28, 32, 32);
    root->setSpacing(18);
    scroll->setWidget(content);
    layout_->addWidget(scroll);

    // ---- Hero
    hero_ = new HeroBanner;
    auto* heroAction = makeButton(tr("办理借阅"), "book-upload", "hero");
    hero_->setAction(heroAction);
    root->addWidget(hero_);

    // ---- Stat tiles
    auto* grid = new QGridLayout;
    grid->setSpacing(16);
    const std::pair<const char*, const char*> tiles[] = {
        {"在借图书", "book-upload"}, {"馆藏品种", "books"}, {"可借册数", "book-2"}, {"逾期未还", "clock"}};
    for (int i = 0; i < 4; ++i) {
        auto* card = new StatCard(tr(tiles[i].first), tiles[i].second);
        grid->addWidget(card, 0, i);
        stats_ << card;
    }
    root->addLayout(grid);

    // ---- Left column: activity + due list
    auto* chartBadge = makeLabel(tr("每日"), "chip");
    auto* chartCard = new Card(tr("借阅趋势"), chartBadge, nullptr, tr("近 %1 天").arg(kDays));
    // The subtitle label is the one with the "hint" role inside the card header.
    for (auto* label : chartCard->findChildren<QLabel*>()) {
        if (label->property("role").toString() == QLatin1String("hint")) chartCaption_ = label;
    }
    chart_ = new ActivityChart;
    chartCard->body()->setContentsMargins(22, 8, 22, 16);
    chartCard->body()->addWidget(chart_);

    auto* allLoans = makeButton(tr("全部借阅"), "arrow-right", "ghost");
    allLoans->setLayoutDirection(Qt::RightToLeft);
    auto* dueCard = new Card(tr("即将到期"), allLoans, nullptr, tr("按应还日期排序 · 双击归还"));
    dueModel_ = new QStandardItemModel(0, 2, this);
    dueTable_ = new DataTable(dueModel_, "circle-check");
    dueTable_->setProperty("embedded", true);
    dueTable_->setEmptyText(tr("没有在借图书"), tr("所有图书都已归还"));
    dueTable_->setItemDelegateForColumn(0, new TwoLineDelegate(dueTable_));
    dueTable_->setItemDelegateForColumn(1, new BadgeDelegate(dueTable_));
    dueTable_->horizontalHeader()->hide();
    dueTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    dueTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    dueTable_->horizontalHeader()->resizeSection(1, 140);
    dueTable_->setSortingEnabled(false);
    dueTable_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    dueCard->body()->addWidget(dueTable_);

    auto* left = new QVBoxLayout;
    left->setSpacing(16);
    left->addWidget(chartCard);
    left->addWidget(dueCard, 1);

    // ---- Right column: ring + ranking
    auto* ringCard = new Card(tr("流通率"), nullptr, nullptr, tr("借出册数 / 馆藏总册数"));
    ring_ = new RingChart;
    ringLegend_ = makeLabel({}, "muted");
    ringLegend_->setAlignment(Qt::AlignCenter);
    ringCard->body()->setContentsMargins(20, 22, 20, 22);
    ringCard->body()->setSpacing(14);
    ringCard->body()->addWidget(ring_, 0, Qt::AlignHCenter);
    ringCard->body()->addWidget(ringLegend_);

    auto* trophy = new QLabel;
    Theme::instance().bindIcon(trophy, "trophy", 18);
    auto* rankCard = new Card(tr("热门图书"), trophy, nullptr, tr("累计借阅次数"));
    ranking_ = rankCard->body();
    ranking_->setContentsMargins(0, 4, 0, 6);

    auto* right = new QVBoxLayout;
    right->setSpacing(16);
    right->addWidget(ringCard);
    right->addWidget(rankCard);
    right->addStretch();

    auto* lower = new QHBoxLayout;
    lower->setSpacing(16);
    lower->addLayout(left, 155);
    lower->addLayout(right, 100);
    root->addLayout(lower, 1);

    connect(heroAction, &QPushButton::clicked, this, &DashboardPage::borrow);
    connect(allLoans, &QPushButton::clicked, this, &DashboardPage::showAllLoansRequested);
    connect(dueTable_, &DataTable::rowActivated, this, &DashboardPage::giveBack);
}

void DashboardPage::showEvent(QShowEvent* event) {
    Page::showEvent(event);
    hero_->replayIntro();
}

void DashboardPage::reload() {
    const auto& service = controller_.service();
    const auto s = service.stats();
    const int lent = s.totalCopies - s.availableCopies;
    const int offset = QDateTime::currentDateTime().offsetFromUtc();
    const auto activityVec = service.borrowActivity(kDays, offset);
    const QList<int> activity(activityVec.begin(), activityVec.end());

    // Hero copy
    const QLocale zh(QLocale::Chinese, QLocale::China);
    hero_->setText(zh.toString(QDate::currentDate(), QStringLiteral("M月d日 dddd")),
                   tr("%1，今天已借出 %2 本").arg(greeting()).arg(activity.isEmpty() ? 0 : activity.last()),
                   s.overdueLoans ? tr("有 %1 本图书逾期未还，记得提醒读者。").arg(s.overdueLoans)
                                  : tr("所有在借图书都在期限内，一切井然有序。"));

    // Stat tiles
    stats_[0]->setValue(s.activeLoans, tr("%1 位读者").arg(s.students));
    stats_[0]->setSpark(activity);
    stats_[1]->setValue(s.titles, tr("共 %1 册").arg(s.totalCopies));
    stats_[2]->setValue(s.availableCopies, tr("已借出 %1 册").arg(lent));
    stats_[3]->setValue(s.overdueLoans, s.overdueLoans ? tr("需尽快催还") : tr("一切正常"), s.overdueLoans > 0);

    // Activity chart
    QStringList labels;
    const QDate today = QDate::currentDate();
    int total = 0;
    for (int i = 0; i < activity.size(); ++i) {
        const QDate day = today.addDays(i - (activity.size() - 1));
        labels << QString::number(day.day());
        total += activity[i];
    }
    chart_->setData(activity, labels);
    if (chartCaption_) chartCaption_->setText(tr("近 %1 天共借出 %2 次").arg(kDays).arg(total));

    // Ring
    ring_->setRatio(s.totalCopies ? qreal(lent) / s.totalCopies : 0);
    ringLegend_->setText(tr("借出 %1 册　·　在馆 %2 册").arg(lent).arg(s.availableCopies));

    // Due list
    const auto loans = service.listLoans({.filter = library::LoanFilter::Active});
    loans_ = QList<library::Loan>(loans.begin(), loans.end());
    dueModel_->removeRows(0, dueModel_->rowCount());
    const auto now = controller_.now();
    for (const auto& loan : loans_.mid(0, 6)) {
        auto* book = new QStandardItem(qs(loan.bookTitle));
        book->setData(tr("%1 · 借于 %2").arg(qs(loan.studentName), formatDate(loan.borrowedAt)), SubtitleRole);
        book->setData(int(Leading::Cover), LeadingRole);
        const int days = daysUntil(loan.dueAt, now);
        auto* due = new QStandardItem(days < 0    ? tr("逾期 %1 天").arg(-days)
                                      : days == 0 ? tr("今天到期")
                                      : days <= 3 ? tr("%1 天后到期").arg(days)
                                                  : formatDate(loan.dueAt));
        due->setData(int(days < 0 ? Tone::Danger : days <= 3 ? Tone::Warning : Tone::Neutral), ToneRole);
        dueModel_->appendRow({book, due});
    }
    dueTable_->setFixedHeight(std::max(1, dueModel_->rowCount()) * 64 + (dueModel_->rowCount() ? 0 : 180));
    dueTable_->revealRows();

    rebuildRanking(service.popularBooks(5));
}

void DashboardPage::rebuildRanking(const std::vector<library::Book>& books) {
    while (auto* item = ranking_->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    int rank = 0;
    for (const auto& book : books) {
        auto* row = new RankRow(++rank, book, nullptr);
        row->onClick = [this, title = qs(book.title)] { emit showBookRequested(title); };
        ranking_->addWidget(row);
        motion::fadeIn(row, 80 * rank, 360);
    }
}

void DashboardPage::borrow() {
    BorrowDialog dialog(controller_, std::nullopt, std::nullopt, this);
    if (dialog.exec() == QDialog::Accepted) {
        emit controller_.notify(tr("《%1》已借给 %2，应还日期 %3")
                                    .arg(qs(dialog.result().bookTitle), qs(dialog.result().studentName),
                                         formatDate(dialog.result().dueAt)));
    }
}

void DashboardPage::giveBack(int row) {
    if (row < 0 || row >= loans_.size()) return;
    const auto loan = loans_[row];
    if (!confirm(this, tr("确认归还"), tr("%1 归还《%2》？").arg(qs(loan.studentName), qs(loan.bookTitle)),
                 tr("确认归还"), false)) {
        return;
    }
    QString error;
    if (controller_.run([&](library::LibraryService& s) { return s.returnBook(loan.id); }, &error)) {
        emit controller_.notify(tr("《%1》已归还").arg(qs(loan.bookTitle)));
    } else {
        emit controller_.notify(error, false);
    }
}

}  // namespace ui
