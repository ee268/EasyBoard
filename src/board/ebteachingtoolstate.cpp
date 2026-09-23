#include "ebteachingtools.h"
#include "ebteachingmetrics.h"

#include <QtMath>

EBTeachingState EBTeachingTools::state(const QRectF &page) const
{
    EBTeachingState result;
    result.kind = _kind;
    result.center = _center - page.topLeft();
    result.frame = _frame.translated(-page.topLeft());
    result.rotation = _rotation;
    result.scale = _scale;
    result.radius = _radius;
    result.measuredAngle = _measuredAngle;
    result.magnification = _magnification;
    result.flipHorizontal = _flipHorizontal;
    result.flipVertical = _flipVertical;
    result.rectangularLens = _rectangularLens;
    return result;
}

void EBTeachingTools::restore(const EBTeachingState &state, const QRectF &page)
{
    setKind(state.kind, page);
    _center = state.center + page.topLeft();
    _frame = state.frame.translated(page.topLeft());
    _rotation = state.rotation;
    _scale = state.scale;
    _radius = state.radius;
    _measuredAngle = state.measuredAngle;
    _magnification = state.magnification;
    _flipHorizontal = state.flipHorizontal;
    _flipVertical = state.flipVertical;
    _rectangularLens = state.rectangularLens;
}

bool EBTeachingTools::closeRequested() const
{
    return _closeRequested;
}

void EBTeachingTools::flip(bool horizontal)
{
    if (horizontal)
        _flipHorizontal = !_flipHorizontal;
    else
        _flipVertical = !_flipVertical;
}

void EBTeachingTools::resetMeasurement()
{
    _measuredAngle = 0.0;
    _rotation = 0.0;
}

void EBTeachingTools::zoomMagnifier(qreal delta)
{
    _magnification = qBound(1.0, _magnification + delta, 8.0);
}

void EBTeachingTools::toggleMagnifierShape()
{
    _rectangularLens = !_rectangularLens;
}

QPointF EBTeachingTools::closePoint() const
{
    if (_kind == Kind::Curtain || _kind == Kind::Spotlight
        || _kind == Kind::Magnifier)
        return _frame.topRight() + QPointF(-12.0, 12.0);
    if (_kind == Kind::Compass)
        return toScene(QPointF(-18.0, -42.0));
    if (_kind == Kind::Protractor)
        return toScene(QPointF(EBTeachingMetrics::ProtractorRadius + 25.0,
                               -38.0));
    if (_kind == Kind::Ruler)
        return toScene(QPointF(EBTeachingMetrics::RulerHalfLength - 20.0,
                               -75.0));
    const QPolygonF triangle = _kind == Kind::Triangle45
        ? EBTeachingMetrics::triangle45() : EBTeachingMetrics::triangle30();
    return toScene(QPointF(triangle.at(2).x() - 12.0,
                           triangle.at(0).y() - 36.0));
}

QPointF EBTeachingTools::resizePoint() const
{
    if (_kind == Kind::Curtain || _kind == Kind::Spotlight
        || _kind == Kind::Magnifier)
        return _frame.bottomRight();
    if (_kind == Kind::Compass)
        return toScene(QPointF(_radius + 14.0, 32.0));
    if (_kind == Kind::Ruler)
        return toScene(EBTeachingMetrics::rulerRect().bottomRight());
    if (_kind == Kind::Protractor)
        return toScene(QPointF(EBTeachingMetrics::ProtractorRadius, 0.0));
    const QPolygonF triangle = _kind == Kind::Triangle45
        ? EBTeachingMetrics::triangle45() : EBTeachingMetrics::triangle30();
    return toScene(triangle.at(2));
}

QPointF EBTeachingTools::rotationPoint() const
{
    if (_kind == Kind::Protractor)
        return toScene(QPointF(0.0, -EBTeachingMetrics::ProtractorRadius - 38.0));
    if (_kind == Kind::Ruler)
        return toScene(QPointF(0.0, -76.0));
    if (_kind == Kind::Triangle45)
        return toScene(EBTeachingMetrics::triangle45().at(0)
                       + QPointF(0.0, -42.0));
    return toScene(EBTeachingMetrics::triangle30().at(0)
                   + QPointF(0.0, -42.0));
}

QPointF EBTeachingTools::compassPencilPoint() const
{
    return toScene(QPointF(_radius, 0.0));
}

QPointF EBTeachingTools::compassHingePoint() const
{
    return toScene(QPointF(_radius / 2.0, 0.0));
}
