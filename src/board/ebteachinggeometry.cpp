#include "ebteachingtools.h"
#include "ebteachingmetrics.h"

#include <QTransform>
#include <QtMath>

QPainterPath EBTeachingTools::toolShape() const
{
    QPainterPath shape;
    if (_kind == Kind::Ruler) {
        shape.addRoundedRect(EBTeachingMetrics::rulerRect(), 6.0, 6.0);
    } else if (_kind == Kind::Triangle45 || _kind == Kind::Triangle30) {
        shape.setFillRule(Qt::OddEvenFill);
        const QPolygonF outer = _kind == Kind::Triangle45
            ? EBTeachingMetrics::triangle45() : EBTeachingMetrics::triangle30();
        const QPolygonF inner = _kind == Kind::Triangle45
            ? EBTeachingMetrics::triangle45Hole()
            : EBTeachingMetrics::triangle30Hole();
        shape.addPolygon(outer);
        shape.closeSubpath();
        shape.addPolygon(inner);
        shape.closeSubpath();
    } else if (_kind == Kind::Protractor) {
        const qreal radius = EBTeachingMetrics::ProtractorRadius;
        shape.moveTo(-radius, 0.0);
        shape.arcTo(QRectF(-radius, -radius, radius * 2.0,
                           radius * 2.0), 180.0, -180.0);
        shape.closeSubpath();
    }
    return shape;
}

QVector<QLineF> EBTeachingTools::edges() const
{
    if (_kind == Kind::Ruler) {
        const QRectF rect = EBTeachingMetrics::rulerRect();
        return {QLineF(rect.topLeft(), rect.topRight()),
                QLineF(rect.bottomLeft(), rect.bottomRight())};
    }
    if (_kind == Kind::Triangle45 || _kind == Kind::Triangle30) {
        const QPolygonF outer = _kind == Kind::Triangle45
            ? EBTeachingMetrics::triangle45() : EBTeachingMetrics::triangle30();
        return {QLineF(outer.at(0), outer.at(1)),
                QLineF(outer.at(1), outer.at(2)),
                QLineF(outer.at(2), outer.at(0))};
    }
    return {};
}

QPointF EBTeachingTools::toLocal(const QPointF &position) const
{
    QTransform transform;
    transform.translate(_center.x(), _center.y());
    transform.rotate(_rotation);
    transform.scale(_flipHorizontal ? -_scale : _scale,
                    _flipVertical ? -_scale : _scale);
    return transform.inverted().map(position);
}

QPointF EBTeachingTools::toScene(const QPointF &position) const
{
    QTransform transform;
    transform.translate(_center.x(), _center.y());
    transform.rotate(_rotation);
    transform.scale(_flipHorizontal ? -_scale : _scale,
                    _flipVertical ? -_scale : _scale);
    return transform.map(position);
}

QPointF EBTeachingTools::constrained(const QPointF &position,
                                    const QRectF &page) const
{
    return QPointF(qBound(page.left(), position.x(), page.right()),
                   qBound(page.top(), position.y(), page.bottom()));
}

qreal EBTeachingTools::angleAt(const QPointF &position) const
{
    return qRadiansToDegrees(qAtan2(_center.y() - position.y(),
                                    position.x() - _center.x()));
}

