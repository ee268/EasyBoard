#ifndef EBTEACHINGMETRICS_H
#define EBTEACHINGMETRICS_H

#include "ebboardscene.h"
#include <QPolygonF>

namespace EBTeachingMetrics {
constexpr qreal Centimeter = EBBoardScene::PatternSpacing;
constexpr qreal RulerHalfLength = 320.0;
constexpr qreal RulerHalfHeight = 42.0;
constexpr qreal ProtractorRadius = 220.0;

inline QRectF rulerRect()
{
    return QRectF(-RulerHalfLength, -RulerHalfHeight,
                  RulerHalfLength * 2.0, RulerHalfHeight * 2.0);
}

inline QPolygonF triangle45()
{
    QPolygonF polygon;
    polygon << QPointF(-210.0, -210.0) << QPointF(-210.0, 210.0)
            << QPointF(210.0, 210.0);
    return polygon;
}

inline QPolygonF triangle45Hole()
{
    QPolygonF polygon;
    polygon << QPointF(-162.0, -94.0) << QPointF(-162.0, 162.0)
            << QPointF(94.0, 162.0);
    return polygon;
}

inline QPolygonF triangle30()
{
    QPolygonF polygon;
    polygon << QPointF(-320.0, -180.0) << QPointF(-320.0, 180.0)
            << QPointF(320.0, 180.0);
    return polygon;
}

inline QPolygonF triangle30Hole()
{
    QPolygonF polygon;
    polygon << QPointF(-270.0, -90.0) << QPointF(-270.0, 130.0)
            << QPointF(121.0, 130.0);
    return polygon;
}
}

#endif
