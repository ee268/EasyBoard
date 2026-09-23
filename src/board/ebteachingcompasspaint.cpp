#include "ebteachingtools.h"
#include "ebteachingmetrics.h"

#include <QPainter>

void EBTeachingTools::paintCompass(QPainter *painter) const
{
    const qreal hinge = _radius / 2.0;
    QPen outline(QColor(76, 81, 84), 1.2);
    outline.setCosmetic(true);
    painter->setPen(outline);

    QPolygonF needleArm;
    needleArm << QPointF(29.0, -10.0)
              << QPointF(hinge - 20.0, -17.0)
              << QPointF(hinge - 20.0, 17.0)
              << QPointF(29.0, 10.0);
    QLinearGradient metal(QPointF(0.0, -18.0), QPointF(0.0, 18.0));
    metal.setColorAt(0.0, QColor(235, 237, 238));
    metal.setColorAt(0.5, QColor(187, 192, 195));
    metal.setColorAt(1.0, QColor(226, 229, 230));
    painter->setBrush(metal);
    painter->drawPolygon(needleArm);

    QPolygonF pencilArm;
    pencilArm << QPointF(hinge + 20.0, -17.0)
              << QPointF(_radius - 29.0, -10.0)
              << QPointF(_radius - 29.0, 10.0)
              << QPointF(hinge + 20.0, 17.0);
    painter->drawPolygon(pencilArm);

    painter->setBrush(QColor(150, 155, 158));
    painter->drawRoundedRect(QRectF(10.0, -9.0, 20.0, 18.0), 3.0, 3.0);
    painter->drawRoundedRect(QRectF(_radius - 31.0, -9.0, 20.0, 18.0),
                             3.0, 3.0);
    painter->setBrush(QColor(91, 95, 98));
    painter->drawPolygon(QPolygonF()
        << QPointF(0.0, 0.0) << QPointF(12.0, -3.5)
        << QPointF(12.0, 3.5));
    painter->setBrush(QColor(37, 42, 47));
    painter->drawPolygon(QPolygonF()
        << QPointF(_radius - 11.0, -4.0)
        << QPointF(_radius, 0.0)
        << QPointF(_radius - 11.0, 4.0));

    QLinearGradient hingeFill(QPointF(0.0, -23.0), QPointF(0.0, 23.0));
    hingeFill.setColorAt(0.0, QColor(230, 232, 233));
    hingeFill.setColorAt(1.0, QColor(151, 156, 160));
    painter->setBrush(hingeFill);
    painter->drawRoundedRect(QRectF(hinge - 22.0, -23.0, 44.0, 46.0),
                             3.0, 3.0);
    painter->setBrush(QColor(194, 198, 200));
    painter->drawEllipse(QPointF(hinge, 0.0), 16.0, 16.0);

    QFont font = painter->font();
    font.setPixelSize(12);
    painter->setFont(font);
    painter->setPen(QColor(49, 54, 57));
    painter->drawText(QRectF(hinge - 24.0, -10.0, 48.0, 20.0),
                      Qt::AlignCenter,
                      QString::number(qRound(qAbs(_sweepAngle)))
                          + QChar(0x00B0));
    painter->drawText(QRectF(hinge + 28.0, -11.0,
                             qMax(30.0, _radius / 2.0 - 55.0), 22.0),
                      Qt::AlignCenter,
                      QString::number(_radius / EBTeachingMetrics::Centimeter,
                                      'f', 1) + QObject::tr(" cm"));
}
