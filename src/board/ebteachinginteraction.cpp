#include "ebteachingtools.h"
#include "ebteachingmetrics.h"

#include <QtMath>

namespace {
constexpr qreal kHitDistance = 12.0;

QPointF nearestPoint(const QLineF &edge, const QPointF &point)
{
    const QPointF delta = edge.p2() - edge.p1();
    const qreal lengthSquared = QPointF::dotProduct(delta, delta);
    if (lengthSquared < 0.001)
        return edge.p1();
    const qreal fraction = qBound(0.0,
        QPointF::dotProduct(point - edge.p1(), delta) / lengthSquared, 1.0);
    return edge.p1() + delta * fraction;
}
}

bool EBTeachingTools::press(const QPointF &position, const QRectF &page,
                            bool controlsVisible)
{
    if (_kind == Kind::None || !page.contains(position))
        return false;
    _lastPosition = position;
    _closeRequested = false;
    if (controlsVisible && QLineF(position, closePoint()).length() <= 14.0) {
        _operation = Operation::Close;
        return true;
    }
    if (controlsVisible && QLineF(position, resizePoint()).length() <= 14.0) {
        _operation = Operation::Resize;
        return true;
    }
    if (_kind == Kind::Curtain || _kind == Kind::Spotlight
        || _kind == Kind::Magnifier) {
        if (_frame.contains(position)) {
            _operation = Operation::Move;
            return true;
        }
        return false;
    }
    if (_kind == Kind::Compass) {
        const QPointF local = toLocal(position);
        if (QLineF(position, compassHingePoint()).length() <= 25.0) {
            _operation = Operation::Rotate;
        } else if (QLineF(position, compassPencilPoint()).length() <= 20.0) {
            _operation = Operation::Draw;
            _startAngle = angleAt(compassPencilPoint());
            _lastAngle = _startAngle;
            _sweepAngle = 0.0;
        } else if (local.x() >= -12.0 && local.x() <= _radius
                   && qAbs(local.y()) <= 24.0)
            _operation = Operation::Move;
        else
            return false;
        return true;
    }
    const QPointF local = toLocal(position);
    if (controlsVisible && QLineF(position, resizePoint()).length() <= 18.0) {
        _operation = Operation::Resize;
        return true;
    }
    if (controlsVisible && QLineF(position, rotationPoint()).length() <= 18.0) {
        _operation = Operation::Rotate;
        return true;
    }
    if (_kind == Kind::Protractor) {
        if (!toolShape().contains(local))
            return false;
        if (QLineF(local, QPointF()).length()
            > EBTeachingMetrics::ProtractorRadius * 0.55) {
            _operation = Operation::Measure;
            _measuredAngle = qBound(0.0,
                qRadiansToDegrees(qAtan2(-local.y(), local.x())), 180.0);
        } else {
            _operation = Operation::Move;
        }
        return true;
    }
    const QVector<QLineF> sides = edges();
    qreal nearest = kHitDistance / _scale;
    _edgeIndex = -1;
    for (int index = 0; index < sides.size(); ++index) {
        const qreal distance = QLineF(local, nearestPoint(sides.at(index), local))
                                   .length();
        if (distance < nearest) {
            nearest = distance;
            _edgeIndex = index;
        }
    }
    if (_edgeIndex >= 0) {
        _operation = Operation::Draw;
        _drawStart = nearestPoint(sides.at(_edgeIndex), local);
        _drawEnd = _drawStart;
        return true;
    }
    if (toolShape().contains(local)) {
        _operation = Operation::Move;
        return true;
    }
    return false;
}

void EBTeachingTools::move(const QPointF &position, const QRectF &page)
{
    if (_operation == Operation::None)
        return;
    const QPointF bounded = constrained(position, page);
    if (_operation == Operation::Move) {
        const QPointF delta = bounded - _lastPosition;
        if (_kind == Kind::Curtain || _kind == Kind::Spotlight
            || _kind == Kind::Magnifier) {
            _frame.translate(delta);
            _frame.moveLeft(qBound(page.left(), _frame.left(),
                                   page.right() - _frame.width()));
            _frame.moveTop(qBound(page.top(), _frame.top(),
                                  page.bottom() - _frame.height()));
        } else
            _center = constrained(_center + delta, page);
    } else if (_operation == Operation::Resize) {
        if (_kind == Kind::Compass)
            _radius = qBound(120.0, toLocal(bounded).x() - 14.0,
                             page.width() / 2.0);
        else if (_kind == Kind::Curtain || _kind == Kind::Spotlight
                 || _kind == Kind::Magnifier)
            _frame.setBottomRight(QPointF(
                qBound(_frame.left() + 80.0, bounded.x(), page.right()),
                qBound(_frame.top() + 60.0, bounded.y(), page.bottom())));
        else {
            const qreal base = _kind == Kind::Ruler ? 323.0
                : _kind == Kind::Protractor ? EBTeachingMetrics::ProtractorRadius
                : _kind == Kind::Triangle45 ? 297.0 : 367.0;
            _scale = qBound(0.5, QLineF(_center, bounded).length() / base, 3.0);
        }
    } else if (_operation == Operation::Rotate) {
        _rotation = qRadiansToDegrees(qAtan2(bounded.y() - _center.y(),
                                             bounded.x() - _center.x()))
                    + (_kind == Kind::Compass ? 0.0 : 90.0);
    } else if (_operation == Operation::Draw) {
        if (_kind == Kind::Compass) {
            const qreal angle = angleAt(bounded);
            qreal delta = angle - _lastAngle;
            if (delta > 180.0)
                delta -= 360.0;
            if (delta < -180.0)
                delta += 360.0;
            _sweepAngle = qBound(-360.0, _sweepAngle + delta, 360.0);
            _lastAngle = angle;
            _rotation = -angle;
        } else {
            _drawEnd = nearestPoint(edges().at(_edgeIndex), toLocal(bounded));
        }
    } else if (_operation == Operation::Measure) {
        const QPointF local = toLocal(bounded);
        _measuredAngle = qBound(0.0,
            qRadiansToDegrees(qAtan2(-local.y(), local.x())), 180.0);
    }
    _lastPosition = bounded;
}

QPainterPath EBTeachingTools::drawingPath(const QPointF &) const
{
    QPainterPath path;
    if (_kind == Kind::Compass) {
        if (qAbs(_sweepAngle) >= 3.0) {
            const QRectF circle(_center.x() - _radius, _center.y() - _radius,
                                _radius * 2.0, _radius * 2.0);
            path.arcMoveTo(circle, _startAngle);
            path.arcTo(circle, _startAngle, _sweepAngle);
        }
    } else if (QLineF(_drawStart, _drawEnd).length() >= 2.0) {
        path.moveTo(toScene(_drawStart));
        path.lineTo(toScene(_drawEnd));
    }
    return path;
}

QPainterPath EBTeachingTools::release(const QPointF &position,
                                     const QRectF &page)
{
    move(position, page);
    const QPainterPath path = _operation == Operation::Draw
        ? drawingPath(position) : QPainterPath();
    _closeRequested = _operation == Operation::Close;
    _operation = Operation::None;
    _edgeIndex = -1;
    return path;
}

bool EBTeachingTools::isInteracting() const
{
    return _operation != Operation::None;
}

void EBTeachingTools::cancel()
{
    _operation = Operation::None;
    _closeRequested = false;
    _edgeIndex = -1;
}

QPainterPath EBTeachingTools::circlePath() const
{
    QPainterPath path;
    if (_kind == Kind::Compass)
        path.addEllipse(_center, _radius, _radius);
    return path;
}

