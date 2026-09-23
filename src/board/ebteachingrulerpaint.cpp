#include "ebteachingtools.h"
#include "ebteachingmetrics.h"

#include <QPainter>

void EBTeachingTools::paintRuler(QPainter *painter) const
{
    const QRectF rect = EBTeachingMetrics::rulerRect();
    QLinearGradient fill(rect.topLeft(), rect.bottomLeft());
    fill.setColorAt(0.0, QColor(238, 240, 241, 220));
    fill.setColorAt(0.22, QColor(201, 205, 208, 220));
    fill.setColorAt(0.5, QColor(184, 190, 194, 222));
    fill.setColorAt(0.78, QColor(201, 205, 208, 220));
    fill.setColorAt(1.0, QColor(238, 240, 241, 220));
    QPen outline(QColor(91, 96, 99), 1.1);
    outline.setCosmetic(true);
    painter->setPen(outline);
    painter->setBrush(fill);
    painter->drawPath(toolShape());

    QFont font = painter->font();
    font.setPixelSize(15);
    painter->setFont(font);
    QPen marks(QColor(65, 69, 72), 0.8);
    marks.setCosmetic(true);
    painter->setPen(marks);
    const qreal step = EBTeachingMetrics::Centimeter / 10.0;
    const qreal start = rect.left() + 12.0;
    for (int millimeter = 0; start + millimeter * step < rect.right() - 8.0;
         ++millimeter) {
        const qreal x = start + millimeter * step;
        const qreal length = millimeter % 10 == 0 ? 18.0
            : millimeter % 5 == 0 ? 12.0 : 6.0;
        painter->drawLine(QPointF(x, rect.top()),
                          QPointF(x, rect.top() + length));
        painter->drawLine(QPointF(x, rect.bottom()),
                          QPointF(x, rect.bottom() - length));
        if (millimeter % 10 == 0 && x < rect.right() - 18.0) {
            const QString number = QString::number(millimeter / 10);
            painter->drawText(QRectF(x - 16.0, rect.top() + 21.0,
                                     32.0, 18.0), Qt::AlignCenter, number);
            painter->drawText(QRectF(x - 16.0, rect.bottom() - 39.0,
                                     32.0, 18.0), Qt::AlignCenter, number);
        }
    }
}
