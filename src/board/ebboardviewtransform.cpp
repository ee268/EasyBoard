#include "ebboardview.h"

#include <QGraphicsItem>
#include <QPainter>
#include <QTransform>
#include <QtMath>

#include <cmath>

#include "../global/ebtheme.h"

namespace {
constexpr qreal kMinObjectScale = 0.25;
constexpr qreal kMaxObjectScale = 4.0;

QPoint rotationHandle(const QVector<QPoint> &frame)
{
    const QPointF top = (QPointF(frame.at(0)) + frame.at(1)) / 2.0;
    const QPointF center = (QPointF(frame.at(0)) + frame.at(2)) / 2.0;
    const QPointF outward = top - center;
    const qreal length = qSqrt(QPointF::dotProduct(outward, outward));
    const QPointF handle = top + (length > 0.0
        ? outward * (24.0 / length) : QPointF(0.0, -24.0));
    return QPoint(qRound(handle.x()), qMax(34, qRound(handle.y())));
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

QVector<QPoint> EBBoardView::selectionFrame() const
{
    const QVector<QGraphicsItem *> objects = _scene->selectedObjects();
    if (objects.size() == 1) {
        QGraphicsItem *object = objects.first();
        const QRectF bounds = object->boundingRect();
        return {mapFromScene(object->mapToScene(bounds.topLeft())),
                mapFromScene(object->mapToScene(bounds.topRight())),
                mapFromScene(object->mapToScene(bounds.bottomRight())),
                mapFromScene(object->mapToScene(bounds.bottomLeft()))};
    }
    if (objects.size() > 1) {
        const qreal angle = objects.first()->rotation();
        bool sameAngle = true;
        for (QGraphicsItem *object : objects) {
            if (qAbs(std::remainder(object->rotation() - angle, 360.0))
                > 0.1) {
                sameAngle = false;
                break;
            }
        }
        if (sameAngle && qAbs(std::remainder(angle, 360.0)) > 0.1) {
            QTransform align;
            align.rotate(-angle);
            QPointF minimum;
            QPointF maximum;
            bool first = true;
            for (QGraphicsItem *object : objects) {
                const QRectF bounds = object->boundingRect();
                const QPointF corners[] = {bounds.topLeft(), bounds.topRight(),
                                           bounds.bottomRight(), bounds.bottomLeft()};
                for (const QPointF &corner : corners) {
                    const QPointF point = align.map(object->mapToScene(corner));
                    if (first) {
                        minimum = point;
                        maximum = point;
                    } else {
                        minimum.setX(qMin(minimum.x(), point.x()));
                        minimum.setY(qMin(minimum.y(), point.y()));
                        maximum.setX(qMax(maximum.x(), point.x()));
                        maximum.setY(qMax(maximum.y(), point.y()));
                    }
                    first = false;
                }
            }
            const QRectF alignedBounds(minimum, maximum);
            QTransform restore;
            restore.rotate(angle);
            return {mapFromScene(restore.map(alignedBounds.topLeft())),
                    mapFromScene(restore.map(alignedBounds.topRight())),
                    mapFromScene(restore.map(alignedBounds.bottomRight())),
                    mapFromScene(restore.map(alignedBounds.bottomLeft()))};
        }
    }
    const QRectF bounds = selectedObjectBounds();
    if (bounds.isEmpty())
        return {};
    const QRect frame = mapFromScene(bounds).boundingRect();
    return {frame.topLeft(), frame.topRight(), frame.bottomRight(),
            frame.bottomLeft()};
}

EBBoardView::TransformHandle EBBoardView::transformHandleAt(
    const QPoint &viewportPosition) const
{
    if (_drawingTool != DrawingTool::Select
        || !_scene->selectedObjectsEditable())
        return TransformHandle::None;
    const QVector<QPoint> frame = selectionFrame();
    if (frame.isEmpty())
        return TransformHandle::None;
    if ((viewportPosition - rotationHandle(frame)).manhattanLength() <= 14)
        return TransformHandle::Rotate;
    for (const QPoint &corner : frame) {
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
    const QVector<QPoint> frame = selectionFrame();
    if (frame.isEmpty())
        return;
    const QColor color = ebThemeColor(EBThemeColor::BoardManualGuide);
    painter->setPen(QPen(color, 1.0, Qt::DashLine));
    painter->setBrush(Qt::NoBrush);
    for (int index = 0; index < frame.size(); ++index)
        painter->drawLine(frame.at(index), frame.at((index + 1) % frame.size()));
    const QPoint rotation = rotationHandle(frame);
    painter->setPen(QPen(color, 1.0));
    painter->drawLine((frame.at(0) + frame.at(1)) / 2, rotation);
    painter->setBrush(Qt::white);
    painter->drawEllipse(rotation, 5, 5);
    for (const QPoint &corner : frame)
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
