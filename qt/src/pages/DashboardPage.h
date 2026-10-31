#pragma once

#include <QList>

#include "pages/Page.h"

class QLabel;
class QStandardItemModel;
class QVBoxLayout;

namespace ui {

class ActivityChart;
class DataTable;
class HeroBanner;
class RingChart;
class StatCard;

class DashboardPage : public Page {
    Q_OBJECT
public:
    DashboardPage(LibraryController& controller, QWidget* parent = nullptr);
    void reload() override;

signals:
    void showAllLoansRequested();
    void showBookRequested(const QString& title);

protected:
    void showEvent(QShowEvent* event) override;

private:
    void borrow();
    void giveBack(int row);
    void rebuildRanking(const std::vector<library::Book>& books);

    HeroBanner* hero_;
    QList<StatCard*> stats_;
    ActivityChart* chart_;
    QLabel* chartCaption_ = nullptr;
    QStandardItemModel* dueModel_;
    DataTable* dueTable_;
    RingChart* ring_;
    QLabel* ringLegend_;
    QVBoxLayout* ranking_;
    QList<library::Loan> loans_;
};

}  // namespace ui
