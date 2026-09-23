#include "ebboardview.h"

#include <QGraphicsItem>
#include <QPainter>
#include <QtMath>

#include "../global/ebtheme.h"

namespace {
constexpr qreal kMinObjectScale = 0.25;
constexpr qreal kMaxObjectScale = 4.0;

QPoint rotationHandle(const QRect &bounds)
{
    return QPoint(bounds.center().x(), qMax(34, bounds.top() - 24));
}
}

QRectF EBBoardView::selectedObjectBounds() const
{
    QRectF bounds;
    for (QGraphicsItem *object : _scene->selectedObjects()) {
        const QRectF itemBounds = object->sceneBoundingRect();
        bounds = bounds.isNull() ? itemBounds : bounds.united(itemBounds);
    }
    return bounds;
}

EBBoardView::TransformHandle EBBoardView::transformHandleAt(
    const QPoint &viewportPosition) const
{
    if (_drawingTool != DrawingTool::Select
        || !_scene->selectedObjectsEditable())
        return TransformHandle::None;
    const QRectF bounds = selectedObjectBounds();
    if (bounds.isEmpty())
        return TransformHandle::None;
    const QRect viewportBounds = mapFromScene(bounds).boundingRect();
    if ((viewportPosition - rotationHandle(viewportBounds)).manhattanLength() <= 14)
        return TransformHandle::Rotate;
    const QPoint corners[] = {viewportBounds.topLeft(), viewportBounds.topRight(),
                              viewportBounds.bottomLeft(), viewportBounds.bottomRight()};
    for (const QPoint &corner : corners) {
        if (qAbs(viewportPosition.x() - corner.x()) <= 8
            && qAbs(viewportPosition.y() - corner.y()) <= 8)
            return TransformHandle::Scale;
    }
    return TransformHandle::None;
}

void EBBoardView::drawSelectionHandles(QPainter *painter)
{
    if (_drawingTool != DrawingTool::Select
        || !_scene->selectedObjectsEditable())
        return;
    const QRectF bounds = selectedObjectBounds();
    if (bounds.isEmpty())
        return;
    const QRect frame = mapFromScene(bounds).boundingRect();
    const QColor color = ebThemeColor(EBThemeColor::BoardManualGuide);
    painter->setPen(QPen(color, 1.0, Qt::DashLine));
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(frame);
    const QPoint rotation = rotationHandle(frame);
    painter->setPen(QPen(color, 1.0));
    painter->drawLine(frame.center().x(), frame.top(), rotation.x(), rotation.y());
    painter->setBrush(Qt::white);
    painter->drawEllipse(rotation, 5, 5);
    const QPoint corners[] = {frame.topLeft(), frame.topRight(),
                              frame.bottomLeft(), frame.bottomRight()};
    for (const QPoint &corner : corners)
        painter->drawRect(QRect(corner.x() - 4, corner.y() - 4, 8, 8));
}

void EBBoardView::startObjectTransform(TransformHandle handle,
                                        const QPointF &scenePosition)
{
    const QVector<QGraphicsItem *> objects = _scene->selectedObjects();
    if (objects.isEmpty() || !_scene->selectedObjectsEditable())
        return;
    _transformHandle = handle;
    _transformStartScene = scenePosition;
    _transformBounds = selectedObjectBounds();
    _transformStates.clear();
    for (QGraphicsItem *object : objects) {
        _transformStates.append({object, object->pos(),
                                 object->sceneBoundingRect().center(),
                                 object->scale(), object->rotation()});
    }
    beginEdit();
    _editDescription = handle == TransformHandle::Scale
        ? tr("拖动缩放对象") : tr("拖动旋转对象");
}

void EBBoardView::updateObjectTransform(const QPointF &scenePosition)
{
    if (_transformHandle == TransformHandle::None)
        return;
    const QPointF center = _transformBounds.center();
    qreal factor = 1.0;
    qreal degrees = 0.0;
    if (_transformHandle == TransformHandle::Scale) {
        const qreal startRadius = QLineF(center, _transformStartScene).length();
        if (startRadius < 1.0)
            return;
        factor = QLineF(center, scenePosition).length() / startRadius;
        qreal minimum = 0.0;
        qreal maximum = 1000.0;
        for (const TransformState &state : _transformStates) {
            minimum = qMax(minimum, kMinObjectScale / state.scale);
            maximum = qMin(maximum, kMaxObjectScale / state.scale);
        }
        factor = qBound(minimum, factor, maximum);
    } else {
        const qreal startAngle = qAtan2(_transformStartScene.y() - center.y(),
                                        _transformStartScene.x() - center.x());
        const qreal currentAngle = qAtan2(scenePosition.y() - center.y(),
                                          scenePosition.x() - center.x());
        degrees = qRadiansToDegrees(currentAngle - startAngle);
    }
    for (const TransformState &state : _transformStates) {
        QGraphicsItem *item = state.item;
        item->setPos(state.position);
        item->setScale(state.scale);
        item->setRotation(state.rotation);
        QPointF targetCenter;
        if (_transformHandle == TransformHandle::Scale) {
            item->setScale(state.scale * factor);
            targetCenter = center + (state.center - center) * factor;
        } else {
            item->setRotation(state.rotation + degrees);
            const qreal angle = qDegreesToRadians(degrees);
            const QPointF offset = state.center - center;
            targetCenter = center + QPointF(offset.x() * qCos(angle)
                                             - offset.y() * qSin(angle),
                                             offset.x() * qSin(angle)
                                             + offset.y() * qCos(angle));
        }
        item->moveBy(targetCenter.x() - item->sceneBoundingRect().center().x(),
                     targetCenter.y() - item->sceneBoundingRect().center().y());
    }
    QVector<QGraphicsItem *> objects;
    for (const TransformState &state : _transformStates)
        objects.append(state.item);
    keepObjectsInsidePage(objects);
    viewport()->update();
}

void EBBoardView::finishObjectTransform()
{
    if (_transformHandle == TransformHandle::None)
        return;
    for (const TransformState &state : _transformStates) {
        const QGraphicsItem *item = state.item;
        _editChanged |= item->pos() != state.position
            || !qFuzzyCompare(item->scale(), state.scale)
            || qAbs(item->rotation() - state.rotation) > 0.001;
    }
    _transformHandle = TransformHandle::None;
    _transformStates.clear();
    finishEdit();
    viewport()->update();
}
