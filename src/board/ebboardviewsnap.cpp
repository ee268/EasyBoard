#include "ebboardview.h"

#include <QGraphicsLineItem>
#include <QtMath>

#include "../global/ebtheme.h"
#include "ebobjectsnap.h"

namespace {
constexpr qreal kSnapDistancePixels = 8.0;

QRectF objectBounds(const QVector<QGraphicsItem *> &objects)
{
    QRectF bounds;
    for (QGraphicsItem *object : objects) {
        const QRectF itemBounds = object->sceneBoundingRect();
        bounds = bounds.isNull() ? itemBounds : bounds.united(itemBounds);
    }
    return bounds;
}
}

void EBBoardView::updateObjectMove(const QPointF &scenePosition,
                                   bool snapEnabled)
{
    if (_movingObjects.isEmpty())
        return;
    const QPointF delta = scenePosition - _moveStartScene;
    for (int index = 0; index < _movingObjects.size(); ++index)
        _movingObjects.at(index)->setPos(_moveStartPositions.at(index) + delta);
    keepObjectsInsidePage(_movingObjects);
    hideSnapGuides();
    if (!snapEnabled || !_snapEnabled)
        return;

    const QRectF moving = objectBounds(_movingObjects);
    const qreal scale = qMax(qAbs(transform().m11()), 0.01);
    const EBObjectSnap::Result snap = EBObjectSnap::calculate(
        moving, pageRect(), _snapReferences, kSnapDistancePixels / scale,
        _gridSnapEnabled && pagePattern() == PagePattern::Grid
            ? EBBoardScene::PatternSpacing : 0.0,
        _scene->verticalGuides(), _scene->horizontalGuides());
    const QPointF correction(snap.horizontal.offset, snap.vertical.offset);
    for (QGraphicsItem *object : _movingObjects)
        object->moveBy(correction.x(), correction.y());
    keepObjectsInsidePage(_movingObjects);

    const QRectF result = objectBounds(_movingObjects);
    QPen guidePen(ebThemeColor(EBThemeColor::BoardAlignmentGuide), 1.0,
                  Qt::DashLine);
    guidePen.setCosmetic(true);
    _verticalGuide->setPen(guidePen);
    _horizontalGuide->setPen(guidePen);
    if (snap.horizontal.matched
        && qAbs(result.left() - moving.left() - snap.horizontal.offset) < 0.5) {
        const QRectF span = snap.horizontal.page || snap.horizontal.grid
            || snap.horizontal.manualGuide
            ? pageRect() : result.united(snap.horizontal.reference);
        _verticalGuide->setLine(snap.horizontal.coordinate, span.top(),
                                snap.horizontal.coordinate, span.bottom());
        _verticalGuide->show();
    }
    if (snap.vertical.matched
        && qAbs(result.top() - moving.top() - snap.vertical.offset) < 0.5) {
        const QRectF span = snap.vertical.page || snap.vertical.grid
            || snap.vertical.manualGuide
            ? pageRect() : result.united(snap.vertical.reference);
        _horizontalGuide->setLine(span.left(), snap.vertical.coordinate,
                                  span.right(), snap.vertical.coordinate);
        _horizontalGuide->show();
    }
}

void EBBoardView::finishObjectMove()
{
    hideSnapGuides();
    if (_movingObjects.isEmpty())
        return;
    for (int index = 0; index < _movingObjects.size(); ++index)
        _editChanged |= _movingObjects.at(index)->pos()
                        != _moveStartPositions.at(index);
    _movingObjects.clear();
    _moveStartPositions.clear();
    _snapReferences.clear();
    finishEdit();
}

void EBBoardView::hideSnapGuides()
{
    if (_verticalGuide)
        _verticalGuide->hide();
    if (_horizontalGuide)
        _horizontalGuide->hide();
}
