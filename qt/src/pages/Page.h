#pragma once

#include <QBoxLayout>
#include <QShowEvent>
#include <QVBoxLayout>
#include <QWidget>

#include "LibraryController.h"

namespace ui {

/// Base for the main pages. Owns the vertical layout and refreshes itself when
/// library data changes — lazily, only while visible, so hidden pages cost nothing.
class Page : public QWidget {
public:
    Page(LibraryController& controller, QWidget* parent) : QWidget(parent), controller_(controller) {
        layout_ = new QVBoxLayout(this);
        layout_->setContentsMargins(32, 28, 32, 28);
        layout_->setSpacing(18);
        connect(&controller_, &LibraryController::dataChanged, this, [this] {
            if (isVisible()) reload();
            else stale_ = true;
        });
    }

    /// Re-reads data from the service and repopulates the view.
    virtual void reload() = 0;

protected:
    void showEvent(QShowEvent* event) override {
        QWidget::showEvent(event);
        if (stale_) {
            stale_ = false;
            reload();
        }
    }

    LibraryController& controller_;
    QVBoxLayout* layout_;

private:
    bool stale_ = true;
};

}  // namespace ui
