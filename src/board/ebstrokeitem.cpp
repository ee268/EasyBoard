#include "ebstrokeitem.h"

#include <QLineF>
#include <QtMath>

#include <algorithm>

EBStrokeItem::EBStrokeItem(const QPainterPath &path, const QPen &pen)
    : QGraphicsPathItem(path)
{
    setPen(pen);
    setTransformOriginPoint(path.boundingRect().center());
}

EBStrokeItem::State EBStrokeItem::state() const
{
    return {path(), pen(), pos(), zValue(), transformOriginPoint(),
            scale(), rotation(), _groupId};
}

void EBStrokeItem::applyState(const State &state)
{
    setPath(state.path);
    setPen(state.pen);
    setPos(state.position);
    setZValue(state.zValue);
    setTransformOriginPoint(state.transformOrigin);
    setScale(state.scale);
    setRotation(state.rotation);
    _groupId = state.groupId;
}

QString EBStrokeItem::groupId() const
{
    return _groupId;
}

void EBStrokeItem::setGroupId(const QString &groupId)
{
    _groupId = groupId;
}

bool EBStrokeItem::splitAt(const QPointF &center, qreal radius,
                           QVector<QPainterPath> &remaining) const
{
    // 当前绘图工具产生折线路径；逐段求与圆形橡皮的交点。
    const QPainterPath original = path();
    QPainterPath fragment;
    QPointF lastPoint;
    bool hasFragment = false;
    bool removedPart = false;
    const qreal radiusSquared = radius * radius;
    const auto flush = [&]() {
        if (hasFragment)
            remaining.append(fragment);
        fragment = QPainterPath();
        hasFragment = false;
    };

    for (int index = 1; index < original.elementCount(); ++index) {
        const QPainterPath::Element previous = original.elementAt(index - 1);
        const QPainterPath::Element current = original.elementAt(index);
        if (current.isMoveTo()) {
            flush();
            continue;
        }
        const QPointF start(previous.x, previous.y);
        const QPointF end(current.x, current.y);
        const QPointF direction = end - start;
        const QPointF offset = start - center;
        const qreal a = QPointF::dotProduct(direction, direction);
        QVector<qreal> cuts{0.0, 1.0};
        if (a > 1e-12) {
            const qreal b = 2.0 * QPointF::dotProduct(offset, direction);
            const qreal c = QPointF::dotProduct(offset, offset) - radiusSquared;
            const qreal discriminant = b * b - 4.0 * a * c;
            if (discriminant > 0.0) {
                const qreal root = qSqrt(discriminant);
                for (qreal t : {(-b - root) / (2.0 * a),
                                (-b + root) / (2.0 * a)}) {
                    if (t > 1e-7 && t < 1.0 - 1e-7)
                        cuts.append(t);
                }
                std::sort(cuts.begin(), cuts.end());
            }
        }

        for (int part = 1; part < cuts.size(); ++part) {
            const qreal from = cuts.at(part - 1);
            const qreal to = cuts.at(part);
            if (to - from <= 1e-7)
                continue;
            const QPointF middle = start + direction * ((from + to) / 2.0);
            const QPointF distance = middle - center;
            if (QPointF::dotProduct(distance, distance) < radiusSquared) {
                // 圆内区间被删除；圆外相邻区间分别成为可保留的笔迹片段。
                removedPart = true;
                flush();
                continue;
            }

            const QPointF first = start + direction * from;
            const QPointF second = start + direction * to;
            if (!hasFragment || QLineF(lastPoint, first).length() > 1e-5) {
                flush();
                fragment.moveTo(first);
                hasFragment = true;
            }
            fragment.lineTo(second);
            lastPoint = second;
        }
    }
    flush();
    return removedPart;
}
