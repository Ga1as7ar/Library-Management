#include "Widgets.h"

#include <QAction>
#include <QGridLayout>
#include <QEnterEvent>
#include <QEvent>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QTimer>
#include <QVBoxLayout>
#include <QtMath>

#include "Animations.h"
#include "Common.h"
#include "Delegates.h"
#include "Painted.h"
#include "Theme.h"

namespace ui {

QPushButton* makeButton(const QString& text, const QString& iconName, const char* variant, QWidget* parent) {
    auto* button = new QPushButton(text, parent);
    button->setCursor(Qt::PointingHandCursor);
    if (variant) button->setProperty("variant", variant);
    if (!iconName.isEmpty()) {
        const QString v = variant ? QString::fromLatin1(variant) : QString();
        Theme::instance().bindIcon(button, iconName, [v](const Palette& p) {
            if (v == "primary") return p.accentText;
            if (v == "danger") return QColor(Qt::white);
            if (v == "hero") return QColor("#0d3f33");
            return p.text2;
        });
        button->setIconSize({16, 16});
    }
    return button;
}

QLabel* makeLabel(const QString& text, const char* role, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setProperty("role", role);
    return label;
}

QHBoxLayout* makePageHeader(QLabel*& title, QLabel*& subtitle, QWidget* action) {
    auto* column = new QVBoxLayout;
    column->setSpacing(4);
    column->addWidget(title);
    column->addWidget(subtitle);

    auto* row = new QHBoxLayout;
    row->addLayout(column);
    row->addStretch();
    if (action) row->addWidget(action, 0, Qt::AlignBottom);
    return row;
}

QLineEdit* makeSearchField(const QString& placeholder, QWidget* parent) {
    auto* edit = new QLineEdit(parent);
    edit->setPlaceholderText(placeholder);
    edit->setClearButtonEnabled(true);
    edit->setMinimumWidth(340);
    auto* action = edit->addAction(QIcon(), QLineEdit::LeadingPosition);
    auto update = [action] { action->setIcon(Theme::instance().icon("search", Theme::instance().palette().text3, 16)); };
    update();
    QObject::connect(&Theme::instance(), &Theme::changed, edit, update);
    return edit;
}

// ================================================================ SegmentedControl

SegmentedControl::SegmentedControl(const QStringList& labels, QWidget* parent) : QFrame(parent) {
    setObjectName("Segmented");
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(3, 3, 3, 3);
    layout->setSpacing(2);
    for (int i = 0; i < labels.size(); ++i) {
        auto* button = new QToolButton(this);
        button->setText(labels[i]);
        button->setCheckable(true);
        button->setProperty("segment", true);
        button->setCursor(Qt::PointingHandCursor);
        connect(button, &QToolButton::clicked, this, [this, i] {
            setCurrentIndex(i);
            emit selected(i);
        });
        layout->addWidget(button);
        buttons_ << button;
    }
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    animation_.setDuration(380);
    animation_.setEasingCurve(QEasingCurve::OutBack);
    connect(&animation_, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
        thumb_ = v.toRectF();
        update();
    });
    connect(&Theme::instance(), &Theme::changed, this, qOverload<>(&QWidget::update));
    setCurrentIndex(0);
}

QRectF SegmentedControl::thumbTarget() const {
    if (buttons_.isEmpty()) return {};
    return QRectF(buttons_[current_]->geometry());
}

void SegmentedControl::setCurrentIndex(int index) {
    current_ = index;
    for (int i = 0; i < buttons_.size(); ++i) buttons_[i]->setChecked(i == index);
    if (!isVisible() || thumb_.isNull()) {
        thumb_ = thumbTarget();
        update();
        return;
    }
    animation_.stop();
    animation_.setStartValue(thumb_);
    animation_.setEndValue(thumbTarget());
    animation_.start();
}

void SegmentedControl::resizeEvent(QResizeEvent* event) {
    QFrame::resizeEvent(event);
    thumb_ = thumbTarget();
}

void SegmentedControl::paintEvent(QPaintEvent* event) {
    QFrame::paintEvent(event);  // background from the stylesheet
    if (thumb_.isNull()) thumb_ = thumbTarget();
    const auto& p = Theme::instance().palette();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath shadow;
    shadow.addRoundedRect(thumb_.translated(0, 1), 6, 6);
    painter.fillPath(shadow, QColor(0, 0, 0, Theme::instance().mode() == Theme::Mode::Dark ? 60 : 18));
    QPainterPath thumb;
    thumb.addRoundedRect(thumb_, 6, 6);
    painter.fillPath(thumb, p.surface);
    painter.setPen(QPen(p.border, 1));
    painter.drawPath(thumb);
}

// ================================================================ Card

Card::Card(const QString& title, QWidget* headerAction, QWidget* parent, const QString& subtitle) : QFrame(parent) {
    setObjectName("Card");
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    if (!title.isEmpty()) {
        auto* head = new QFrame(this);
        head->setObjectName("CardHead");
        auto* headLayout = new QHBoxLayout(head);
        headLayout->setContentsMargins(22, 16, 18, 16);
        auto* texts = new QVBoxLayout;
        texts->setSpacing(2);
        texts->addWidget(makeLabel(title, "h2"));
        if (!subtitle.isEmpty()) texts->addWidget(makeLabel(subtitle, "hint"));
        headLayout->addLayout(texts);
        headLayout->addStretch();
        if (headerAction) headLayout->addWidget(headerAction);
        outer->addWidget(head);
    }
    body_ = new QVBoxLayout;
    body_->setContentsMargins(0, 0, 0, 0);
    body_->setSpacing(0);
    outer->addLayout(body_, 1);
}

// ================================================================ StatCard

StatCard::StatCard(const QString& label, const QString& icon, QWidget* parent)
    : QFrame(parent), iconName_(icon), shadow_(new QGraphicsDropShadowEffect(this)) {
    setObjectName("Card");
    setMinimumHeight(134);
    auto* layout = new QGridLayout(this);
    layout->setContentsMargins(22, 20, 22, 20);
    layout->setVerticalSpacing(6);

    icon_ = new QLabel(this);
    icon_->setObjectName("StatIcon");
    icon_->setFixedSize(36, 36);
    icon_->setAlignment(Qt::AlignCenter);
    value_ = makeLabel("0", "statValue", this);
    caption_ = makeLabel({}, "muted", this);
    layout->addWidget(makeLabel(label, "muted", this), 0, 0);
    layout->addWidget(icon_, 0, 1, 2, 1, Qt::AlignRight | Qt::AlignTop);
    layout->addWidget(value_, 1, 0);
    layout->addWidget(caption_, 2, 0, 1, 2);
    layout->setRowStretch(3, 1);

    refreshIcon();
    connect(&Theme::instance(), &Theme::changed, this, &StatCard::refreshIcon);

    shadow_->setOffset(0, 2);
    shadow_->setBlurRadius(8);
    setGraphicsEffect(shadow_);
    hover_.setDuration(260);
    hover_.setEasingCurve(QEasingCurve::OutCubic);
    connect(&hover_, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
        const qreal t = v.toReal();
        shadow_->setBlurRadius(8 + 26 * t);
        shadow_->setOffset(0, 2 + 8 * t);
    });
}

void StatCard::refreshIcon() {
    const auto& p = Theme::instance().palette();
    icon_->setPixmap(Theme::instance().icon(iconName_, alert_ ? p.danger : p.accent, 18).pixmap(18, 18));
    icon_->setProperty("alert", alert_);
    icon_->style()->unpolish(icon_);
    icon_->style()->polish(icon_);
    const bool dark = Theme::instance().mode() == Theme::Mode::Dark;
    QColor shadow = dark ? QColor(0, 0, 0) : QColor(18, 40, 33);
    shadow.setAlpha(dark ? 90 : 18);
    shadow_->setColor(shadow);
}

void StatCard::setValue(int value, const QString& caption, bool alert) {
    alert_ = alert;
    value_->setProperty("alert", alert);
    value_->style()->unpolish(value_);
    value_->style()->polish(value_);
    caption_->setText(caption);
    motion::countUp(value_, value);
    refreshIcon();
}

void StatCard::setSpark(const QList<int>& values) {
    if (!spark_) {
        spark_ = new Sparkline(this);
        spark_->lower();
    }
    spark_->setGeometry(0, height() - 44, width(), 44);
    spark_->setValues(values);
    spark_->show();
}

void StatCard::resizeEvent(QResizeEvent* event) {
    QFrame::resizeEvent(event);
    if (spark_) spark_->setGeometry(1, height() - 45, width() - 2, 44);
}

void StatCard::enterEvent(QEnterEvent* event) {
    QFrame::enterEvent(event);
    hover_.stop();
    hover_.setStartValue(hover_.currentValue().isValid() ? hover_.currentValue() : 0.0);
    hover_.setEndValue(1.0);
    hover_.start();
}

void StatCard::leaveEvent(QEvent* event) {
    QFrame::leaveEvent(event);
    hover_.stop();
    hover_.setStartValue(hover_.currentValue().isValid() ? hover_.currentValue() : 1.0);
    hover_.setEndValue(0.0);
    hover_.start();
}

// ================================================================ EmptyState

EmptyState::EmptyState(const QString& iconName, QWidget* parent)
    : QWidget(parent), iconName_(iconName), float_(new QTimer(this)) {
    auto* layout = new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(6);
    layout->addSpacing(84);  // room for the painted illustration
    title_ = makeLabel({}, "emptyTitle", this);
    text_ = makeLabel({}, "muted", this);
    for (auto* label : {title_, text_}) label->setAlignment(Qt::AlignCenter);
    layout->addWidget(title_);
    layout->addWidget(text_);
    setAttribute(Qt::WA_TransparentForMouseEvents);

    float_->setInterval(33);
    connect(float_, &QTimer::timeout, this, [this] {
        phase_ += 0.05;
        update(rect().center().x() - 60, 0, 120, height() / 2 + 20);
    });
    float_->start();
}

void EmptyState::setText(const QString& title, const QString& text) {
    title_->setText(title);
    text_->setText(text);
}

void EmptyState::paintEvent(QPaintEvent*) {
    if (!isVisible()) return;
    const auto& p = Theme::instance().palette();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const qreal dy = 4 * qSin(phase_);
    const QPointF center(width() / 2.0, title_->y() - 56 + dy);
    QRectF tile(center.x() - 32, center.y() - 32, 64, 64);

    painter.save();
    painter.translate(tile.center());
    painter.rotate(-3 * qSin(phase_ * 0.8));
    painter.translate(-tile.center());
    QPainterPath path;
    path.addRoundedRect(tile, 20, 20);
    painter.fillPath(path, p.accentSoft);
    const QPixmap glyph = Theme::instance().icon(iconName_, p.accent, 30).pixmap(30, 30);
    painter.drawPixmap(QPointF(tile.center().x() - 15, tile.center().y() - 15), glyph);
    painter.restore();

    // Soft shadow ellipse that breathes with the float.
    QColor shade = p.text;
    shade.setAlpha(Theme::instance().mode() == Theme::Mode::Dark ? 40 : 16);
    painter.setPen(Qt::NoPen);
    painter.setBrush(shade);
    const qreal w = 40 - 6 * qSin(phase_);
    painter.drawEllipse(QPointF(center.x(), title_->y() - 12), w / 2, 3);
}

// ================================================================ DataTable

DataTable::DataTable(QStandardItemModel* model, const QString& emptyIcon, QWidget* parent)
    : QTableView(parent), model_(model), proxy_(new QSortFilterProxyModel(this)), empty_(new EmptyState(emptyIcon, viewport())) {
    proxy_->setSourceModel(model_);
    proxy_->setSortRole(SortRole);
    proxy_->setSortCaseSensitivity(Qt::CaseInsensitive);
    proxy_->setSortLocaleAware(true);
    setModel(proxy_);

    setSelectionBehavior(SelectRows);
    setSelectionMode(SingleSelection);
    setEditTriggers(NoEditTriggers);
    setSortingEnabled(true);
    setShowGrid(false);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setWordWrap(false);
    setContextMenuPolicy(Qt::CustomContextMenu);
    setHorizontalScrollMode(ScrollPerPixel);
    setVerticalScrollMode(ScrollPerPixel);
    setItemDelegate(new RowDelegate(this));
    verticalHeader()->hide();
    verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    verticalHeader()->setDefaultSectionSize(64);
    horizontalHeader()->setHighlightSections(false);
    horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    horizontalHeader()->setMinimumSectionSize(60);
    horizontalHeader()->setFixedHeight(40);

    const auto toSource = [this](const QModelIndex& index) { return index.isValid() ? proxy_->mapToSource(index).row() : -1; };
    connect(this, &QTableView::doubleClicked, this, [=, this](const QModelIndex& index) { emit rowActivated(toSource(index)); });
    connect(this, &QWidget::customContextMenuRequested, this, [=, this](const QPoint& pos) {
        const QModelIndex index = indexAt(pos);
        if (!index.isValid()) return;
        selectRow(index.row());
        emit contextMenuForRow(toSource(index), viewport()->mapToGlobal(pos));
    });
    connect(selectionModel(), &QItemSelectionModel::currentRowChanged, this,
            [=, this](const QModelIndex& current) { emit currentRowChanged(toSource(current)); });
    connect(model_, &QAbstractItemModel::modelReset, this, &DataTable::updateEmptyState);
    connect(model_, &QAbstractItemModel::rowsInserted, this, &DataTable::updateEmptyState);
    connect(model_, &QAbstractItemModel::rowsRemoved, this, &DataTable::updateEmptyState);
    // Delegates read palette colours directly, so repaint on theme switch.
    connect(&Theme::instance(), &Theme::changed, viewport(), qOverload<>(&QWidget::update));
    updateEmptyState();
}

int DataTable::currentSourceRow() const {
    const QModelIndex index = currentIndex();
    return index.isValid() && selectionModel()->isRowSelected(index.row(), {}) ? proxy_->mapToSource(index).row() : -1;
}

void DataTable::setEmptyText(const QString& title, const QString& text) { empty_->setText(title, text); }

void DataTable::revealRows() { motion::fadeIn(viewport(), 0, 280); }

void DataTable::resizeEvent(QResizeEvent* event) {
    QTableView::resizeEvent(event);
    empty_->setGeometry(viewport()->rect());
}

void DataTable::updateEmptyState() {
    const bool empty = model_->rowCount() == 0;
    if (empty && !empty_->isVisible()) {
        empty_->setGeometry(viewport()->rect());
        empty_->show();
        motion::fadeIn(empty_, 60, 360);
    } else if (!empty) {
        empty_->hide();
    }
}

// ================================================================ Toast

Toast::Toast(QWidget* parent) : QFrame(parent), timer_(new QTimer(this)) {
    setObjectName("Toast");
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(16, 12, 20, 14);
    layout->setSpacing(10);
    icon_ = new QLabel(this);
    text_ = new QLabel(this);
    text_->setWordWrap(true);
    text_->setMaximumWidth(400);
    layout->addWidget(icon_);
    layout->addWidget(text_, 1);
    motion::applyShadow(this, 40, 12, 40);

    slide_.setEasingCurve(QEasingCurve::OutBack);
    connect(&slide_, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
        lift_ = v.toReal();
        reposition(lift_);
    });
    connect(&slide_, &QVariantAnimation::finished, this, [this] {
        if (lift_ <= 0.01) hide();
    });
    drain_.setStartValue(1.0);
    drain_.setEndValue(0.0);
    connect(&drain_, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
        remaining_ = v.toReal();
        update();
    });

    timer_->setSingleShot(true);
    connect(timer_, &QTimer::timeout, this, [this] {
        slide_.stop();
        slide_.setEasingCurve(QEasingCurve::InCubic);
        slide_.setDuration(240);
        slide_.setStartValue(lift_);
        slide_.setEndValue(0.0);
        slide_.start();
    });
    parent->installEventFilter(this);
    hide();
}

void Toast::show(const QString& message, bool success) {
    success_ = success;
    const auto& p = Theme::instance().palette();
    icon_->setPixmap(Theme::instance()
                         .icon(success ? "circle-check" : "alert-triangle", success ? p.accent : p.danger, 20)
                         .pixmap(20, 20));
    text_->setText(message);
    adjustSize();
    raise();
    QFrame::show();

    slide_.stop();
    slide_.setEasingCurve(QEasingCurve::OutBack);
    slide_.setDuration(460);
    slide_.setStartValue(lift_);
    slide_.setEndValue(1.0);
    slide_.start();
    drain_.stop();
    drain_.setDuration(3400);
    drain_.start();
    timer_->start(3400);
}

void Toast::paintEvent(QPaintEvent* event) {
    QFrame::paintEvent(event);
    const auto& p = Theme::instance().palette();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QColor bar = success_ ? p.accent : p.danger;
    bar.setAlpha(150);
    QPainterPath clip;
    clip.addRoundedRect(QRectF(rect()).adjusted(1, 1, -1, -1), 11, 11);
    painter.setClipPath(clip);
    painter.fillRect(QRectF(0, height() - 3, width() * remaining_, 3), bar);
}

void Toast::reposition(qreal lift) {
    const QWidget* host = parentWidget();
    const int restY = host->height() - height() - 28;
    move(host->width() - width() - 28, int(restY + (1 - lift) * (height() + 40)));
}

bool Toast::eventFilter(QObject* watched, QEvent* event) {
    if (watched == parentWidget() && event->type() == QEvent::Resize) reposition(lift_);
    return QFrame::eventFilter(watched, event);
}

}  // namespace ui
