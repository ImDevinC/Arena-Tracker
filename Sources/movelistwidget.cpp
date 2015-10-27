#include "movelistwidget.h"
#include "deckcard.h"
#include <QtWidgets>

MoveListWidget::MoveListWidget(QWidget *parent) : QListWidget(parent)
{
    this->setFrameShape(QFrame::NoFrame);
    this->setIconSize(CARD_SIZE);
    this->setStyleSheet("QListView{background-color: transparent;}"
                                        "QListView::item{padding: -1px;}");

    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    this->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    this->setSizeAdjustPolicy(QAbstractScrollArea::AdjustIgnored);
    this->setSelectionBehavior(QAbstractItemView::SelectRows);
    this->setSelectionMode(QAbstractItemView::NoSelection);
}


void MoveListWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    QListWidget::mouseDoubleClickEvent(event);
    event->ignore();
}
void MoveListWidget::mouseMoveEvent(QMouseEvent *event)
{
    QListWidget::mouseMoveEvent(event);
    event->ignore();
}
void MoveListWidget::mousePressEvent(QMouseEvent *event)
{
    QListWidget::mousePressEvent(event);
    event->ignore();
}
void MoveListWidget::mouseReleaseEvent(QMouseEvent *event)
{
    QListWidget::mouseReleaseEvent(event);
    event->ignore();
}
