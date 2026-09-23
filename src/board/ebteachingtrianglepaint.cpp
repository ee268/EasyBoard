#include "ebteachingtools.h"
#include "ebteachingmetrics.h"

#include <QPainter>

void EBTeachingTools::paintTriangle(QPainter *painter) const
{
    const QPolygonF outer = _kind == Kind::Triangle45
        ? EBTeachingMetrics::triangle45() : EBTeachingMetrics::triangle30();
    const QPolygonF inner = _kind == Kind::Triangle45
        ? EBTeachingMetrics::triangle45Hole()
        : EBTeachingMetrics::triangle30Hole();
    QLinearGradient fill(outer.at(0), outer.at(2));
    fill.setColorAt(0.0, QColor(218, 220, 222, 235));
    fill.setColorAt(0.5, QColor(198, 201, 203, 235));
    fill.setColorAt(1.0, QColor(181, 185, 188, 235));
    QPen outline(QColor(91, 96, 99), 1.1);
    outline.setCosmetic(true);
    painter->setPen(outline);
    painter->setBrush(fill);
    painter->drawPath(toolShape());
    painter->setBrush(Qt::NoBrush);
    painter->drawPolygon(inner);

    QFont font = painter->font();
    font.setPixelSize(15);
    painter->setFont(font);
    QPen marks(QColor(65, 69, 72), 0.8);
    marks.setCosmetic(true);
    painter->setPen(marks);
    const qreal bottom = outer.at(1).y();
    const qreal start = outer.at(1).x() + 10.0;
    const qreal step = EBTeachingMetrics::Centimeter / 10.0;
    for (int millimeter = 0; start + millimeter * step < outer.at(2).x() - 10.0;
         ++millimeter) {
        const qreal x = start + millimeter * step;
        const qreal length = millimeter % 10 == 0 ? 18.0
            : millimeter % 5 == 0 ? 12.0 : 6.0;
        painter->drawLine(QPointF(x, bottom), QPointF(x, bottom - length));
        if (millimeter % 10 == 0 && x < outer.at(2).x() - 25.0)
            painter->drawText(QRectF(x - 15.0, bottom - 42.0, 30.0, 18.0),
                              Qt::AlignCenter, QString::number(millimeter / 10));
    }
}
