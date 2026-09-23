#include "ebboardview.h"

#include <QGraphicsLineItem>
#include <QPainter>
#include <QtMath>

#include "../global/ebtheme.h"

namespace {
constexpr int kRulerWidth = 24;
constexpr qreal kGuideHitPixels = 6.0;
constexpr int kMaxGuidesPerAxis = 100;

QLineF guideLine(const QRectF &page, bool horizontal, qreal value)
{
    if (horizontal)
        return QLineF(page.left(), page.top() + value,
                      page.right(), page.top() + value);
    return QLineF(page.left() + value, page.top(),
                  page.left() + value, page.bottom());
}
}

void EBBoardView::refreshManualGuides()
{
    for (QGraphicsLineItem *item : _manualGuideItems)
        delete item;
    _manualGuideItems.clear();
    QPen pen(ebThemeColor(EBThemeColor::BoardManualGuide), 1.0,
             Qt::DashLine);
    pen.setCosmetic(true);
    const QRectF page = pageRect();
    for (qreal value : _scene->horizontalGuides()) {
        QGraphicsLineItem *item = _scene->addLine(guideLine(page, true, value), pen);
        item->setZValue(1000000.5);
        item->setAcceptedMouseButtons(Qt::NoButton);
        _manualGuideItems.append(item);
    }
    for (qreal value : _scene->verticalGuides()) {
        QGraphicsLineItem *item = _scene->addLine(guideLine(page, false, value), pen);
        item->setZValue(1000000.5);
        item->setAcceptedMouseButtons(Qt::NoButton);
        _manualGuideItems.append(item);
    }
    viewport()->update();
}

bool EBBoardView::guideAt(const QPoint &viewportPosition,
                          GuideAxis *axis, int *index) const
{
    if (!pageRect().contains(mapToScene(viewportPosition)))
        return false;
    qreal nearest = kGuideHitPixels;
    const QRectF page = pageRect();
    for (int row = 0; row < _scene->horizontalGuides().size(); ++row) {
        const int y = mapFromScene(page.left(),
            page.top() + _scene->horizontalGuides().at(row)).y();
        const qreal distance = qAbs(viewportPosition.y() - y);
        if (distance <= nearest) {
            nearest = distance;
            *axis = GuideAxis::Horizontal;
            *index = row;
        }
    }
    for (int column = 0; column < _scene->verticalGuides().size(); ++column) {
        const int x = mapFromScene(page.left()
            + _scene->verticalGuides().at(column), page.top()).x();
        const qreal distance = qAbs(viewportPosition.x() - x);
        if (distance <= nearest) {
            nearest = distance;
            *axis = GuideAxis::Vertical;
            *index = column;
        }
    }
    return *axis != GuideAxis::None;
}

void EBBoardView::startGuideDrag(GuideAxis axis, int index, qreal value)
{
    _guideAxis = axis;
    _guideIndex = index;
    _guideValue = value;
    beginEdit();
    _editDescription = tr("编辑参考线");
    if (index >= 0) {
        const int itemIndex = axis == GuideAxis::Horizontal
            ? index : _scene->horizontalGuides().size() + index;
        _manualGuideItems.at(itemIndex)->hide();
    }
    _guidePreview->setLine(guideLine(pageRect(), axis == GuideAxis::Horizontal,
                                     value));
    _guidePreview->setVisible(index >= 0);
}

void EBBoardView::updateGuideDrag(const QPointF &scenePosition)
{
    if (_guideAxis == GuideAxis::None)
        return;
    const QRectF page = pageRect();
    _guideValue = _guideAxis == GuideAxis::Horizontal
        ? qBound(0.0, scenePosition.y() - page.top(), page.height())
        : qBound(0.0, scenePosition.x() - page.left(), page.width());
    _guidePreview->setLine(guideLine(page,
        _guideAxis == GuideAxis::Horizontal, _guideValue));
    _guidePreview->setVisible(page.contains(scenePosition));
}

void EBBoardView::finishGuideDrag(bool commit)
{
    if (_guideAxis == GuideAxis::None)
        return;
    if (commit) {
        QVector<qreal> horizontal = _scene->horizontalGuides();
        QVector<qreal> vertical = _scene->verticalGuides();
        QVector<qreal> *guides = _guideAxis == GuideAxis::Horizontal
            ? &horizontal : &vertical;
        if (_guideIndex >= 0) {
            if (qAbs(guides->at(_guideIndex) - _guideValue) > 0.001) {
                (*guides)[_guideIndex] = _guideValue;
                _editChanged = true;
            }
        } else if (guides->size() < kMaxGuidesPerAxis) {
            guides->append(_guideValue);
            _editChanged = true;
        }
        if (_editChanged)
            _scene->setGuides(horizontal, vertical);
    }
    _guidePreview->hide();
    _guideAxis = GuideAxis::None;
    _guideIndex = -1;
    refreshManualGuides();
    finishEdit();
}

void EBBoardView::removeGuide(GuideAxis axis, int index)
{
    beginEdit();
    _editDescription = tr("删除参考线");
    QVector<qreal> horizontal = _scene->horizontalGuides();
    QVector<qreal> vertical = _scene->verticalGuides();
    if (axis == GuideAxis::Horizontal)
        horizontal.removeAt(index);
    else
        vertical.removeAt(index);
    _scene->setGuides(horizontal, vertical);
    _editChanged = true;
    refreshManualGuides();
    finishEdit();
}

void EBBoardView::drawForeground(QPainter *painter, const QRectF &rect)
{
    QGraphicsView::drawForeground(painter, rect);
    painter->save();
    painter->resetTransform();
    const int width = viewport()->width();
    const int height = viewport()->height();
    painter->fillRect(QRect(0, 0, width, kRulerWidth),
                      ebThemeColor(EBThemeColor::BoardRulerBackground));
    painter->fillRect(QRect(0, 0, kRulerWidth, height),
                      ebThemeColor(EBThemeColor::BoardRulerBackground));
    painter->setPen(ebThemeColor(EBThemeColor::BoardRulerText));
    QFont labelFont = painter->font();
    labelFont.setPixelSize(9);
    painter->setFont(labelFont);
    const QRectF page = pageRect();
    for (int value = 0; value <= page.width(); value += 25) {
        const int x = mapFromScene(page.left() + value, page.top()).x();
        if (x < kRulerWidth || x > width)
            continue;
        const int tick = value % 100 == 0 ? 9 : 5;
        painter->drawLine(x, kRulerWidth - tick, x, kRulerWidth - 1);
        if (value % 100 == 0)
            painter->drawText(QRect(x + 2, 1, 34, 13),
                              Qt::AlignLeft | Qt::AlignVCenter,
                              QString::number(value));
    }
    for (int value = 0; value <= page.height(); value += 25) {
        const int y = mapFromScene(page.left(), page.top() + value).y();
        if (y < kRulerWidth || y > height)
            continue;
        const int tick = value % 100 == 0 ? 9 : 5;
        painter->drawLine(kRulerWidth - tick, y,
                          kRulerWidth - 1, y);
        if (value % 100 == 0)
            painter->drawText(QRect(1, y + 2, 21, 13),
                              Qt::AlignCenter, QString::number(value));
    }
    painter->restore();
}
