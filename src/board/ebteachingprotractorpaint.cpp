#include "ebteachingtools.h"
#include "ebteachingmetrics.h"

#include <QPainter>
#include <QtMath>

void EBTeachingTools::paintProtractor(QPainter *painter) const
{
    const qreal radius = EBTeachingMetrics::ProtractorRadius;
    QLinearGradient fill(QPointF(0.0, -radius), QPointF(0.0, 0.0));
    fill.setColorAt(0.0, QColor(242, 243, 243, 225));
    fill.setColorAt(1.0, QColor(199, 203, 205, 224));
    QPen outline(QColor(85, 90, 93), 1.1);
    outline.setCosmetic(true);
    painter->setPen(outline);
    painter->setBrush(fill);
    painter->drawPath(toolShape());

    const qreal innerRadius = radius * 0.5;
    QPainterPath inner;
    inner.moveTo(-innerRadius, 0.0);
    inner.arcTo(QRectF(-innerRadius, -innerRadius,
                       innerRadius * 2.0, innerRadius * 2.0), 180.0, -180.0);
    inner.closeSubpath();
    QLinearGradient innerFill(QPointF(0.0, -innerRadius), QPointF());
    innerFill.setColorAt(0.0, QColor(186, 191, 194, 210));
    innerFill.setColorAt(1.0, QColor(138, 145, 149, 212));
    painter->setBrush(innerFill);
    painter->drawPath(inner);
    painter->setBrush(Qt::NoBrush);

    QPen marks(QColor(66, 70, 73), 0.8);
    marks.setCosmetic(true);
    painter->setPen(marks);
    for (int degree = 0; degree <= 180; ++degree) {
        const qreal radians = qDegreesToRadians(qreal(degree));
        const qreal cosine = qCos(radians);
        const qreal sine = qSin(radians);
        const qreal length = degree % 10 == 0 ? 20.0
            : degree % 5 == 0 ? 13.0 : 6.0;
        painter->drawLine(QPointF(radius * cosine, -radius * sine),
                          QPointF((radius - length) * cosine,
                                  -(radius - length) * sine));
        painter->drawLine(QPointF(innerRadius * cosine,
                                  -innerRadius * sine),
                          QPointF((innerRadius + length) * cosine,
                                  -(innerRadius + length) * sine));
        if (degree > 0 && degree < 180 && degree % 10 == 0) {
            QFont font = painter->font();
            font.setPixelSize(17);
            painter->setFont(font);
            const QPointF outer((radius - 39.0) * cosine,
                                 -(radius - 39.0) * sine);
            painter->drawText(QRectF(outer - QPointF(21.0, 12.0),
                                     QSizeF(42.0, 24.0)),
                              Qt::AlignCenter, QString::number(degree));
            font.setPixelSize(12);
            painter->setFont(font);
            const QPointF inside((innerRadius + 38.0) * cosine,
                                  -(innerRadius + 38.0) * sine);
            painter->drawText(QRectF(inside - QPointF(18.0, 9.0),
                                     QSizeF(36.0, 18.0)),
                              Qt::AlignCenter, QString::number(180 - degree));
        }
    }
    painter->drawLine(QPointF(-radius, 0.0), QPointF(radius, 0.0));
    painter->drawLine(QPointF(0.0, 0.0), QPointF(0.0, -20.0));
    painter->setPen(QPen(QColor(32, 115, 154), 1.3));
    const qreal marker = qDegreesToRadians(_measuredAngle);
    painter->drawLine(QPointF(),
        QPointF((radius + 12.0) * qCos(marker),
                -(radius + 12.0) * qSin(marker)));
    painter->setBrush(Qt::white);
    painter->drawEllipse(QPointF(), 4.0, 4.0);
}
