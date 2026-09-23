#include "ebteachingtools.h"
#include "ebteachingmetrics.h"

#include <QGraphicsScene>
#include <QImage>
#include <QPainter>
#include <QtMath>

namespace {
QPen controlPen(qreal width = 1.5)
{
    QPen pen(QColor(55, 83, 94), width);
    pen.setCosmetic(true);
    return pen;
}
}

void EBTeachingTools::paint(QPainter *painter, QGraphicsScene *scene,
                            const QRectF &page, bool active) const
{
    if (_kind == Kind::None)
        return;
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    if (_kind == Kind::Curtain) {
        painter->setPen(controlPen(2.0));
        painter->setBrush(QColor(42, 58, 75, 245));
        painter->drawRect(_frame);
        painter->setPen(Qt::white);
        painter->drawText(_frame, Qt::AlignCenter, QObject::tr("拖动幕布，右下角调整大小"));
    } else if (_kind == Kind::Spotlight) {
        QPainterPath darkness;
        darkness.setFillRule(Qt::OddEvenFill);
        darkness.addRect(page);
        darkness.addEllipse(_frame);
        painter->fillPath(darkness, QColor(22, 32, 43, 210));
        painter->setPen(controlPen(2.0));
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(_frame);
    } else if (_kind == Kind::Magnifier) {
        const QRectF sample(_frame.center().x() - _frame.width()
                                / (2.0 * _magnification),
                            _frame.center().y() - _frame.height()
                                / (2.0 * _magnification),
                            _frame.width() / _magnification,
                            _frame.height() / _magnification);
        const QSize imageSize(qMax(1, qCeil(_frame.width() * 2.0)),
                              qMax(1, qCeil(_frame.height() * 2.0)));
        QImage image(imageSize, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::white);
        QPainter imagePainter(&image);
        scene->render(&imagePainter, QRectF(QPointF(), imageSize), sample,
                      Qt::IgnoreAspectRatio);
        imagePainter.end();
        QPainterPath lens;
        if (_rectangularLens)
            lens.addRoundedRect(_frame, 8.0, 8.0);
        else
            lens.addEllipse(_frame);
        painter->setClipPath(lens);
        painter->drawImage(_frame, image);
        painter->setClipping(false);
        painter->setPen(controlPen(3.0));
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(lens);
    } else {
        painter->save();
        painter->translate(_center);
        painter->rotate(_rotation);
        if (_kind != Kind::Compass)
            painter->scale(_flipHorizontal ? -_scale : _scale,
                           _flipVertical ? -_scale : _scale);
        if (_kind == Kind::Ruler)
            paintRuler(painter);
        else if (_kind == Kind::Triangle45 || _kind == Kind::Triangle30)
            paintTriangle(painter);
        else if (_kind == Kind::Protractor)
            paintProtractor(painter);
        else if (_kind == Kind::Compass)
            paintCompass(painter);
        painter->restore();
    }

    if (_operation == Operation::Draw) {
        painter->setPen(QPen(QColor(21, 112, 157), 2.0));
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(drawingPath(_lastPosition));
    }
    if (active) {
        painter->setPen(controlPen());
        painter->setBrush(Qt::white);
        const QPointF close = closePoint();
        painter->drawEllipse(close, 10.0, 10.0);
        painter->drawLine(close + QPointF(-4.0, -4.0),
                          close + QPointF(4.0, 4.0));
        painter->drawLine(close + QPointF(-4.0, 4.0),
                          close + QPointF(4.0, -4.0));
        const QPointF resize = resizePoint();
        painter->drawRect(QRectF(resize - QPointF(7.0, 7.0),
                                 QSizeF(14.0, 14.0)));
        if (_kind == Kind::Ruler || _kind == Kind::Triangle45
            || _kind == Kind::Triangle30 || _kind == Kind::Protractor) {
            const QPointF rotate = rotationPoint();
            QPointF anchor;
            if (_kind == Kind::Ruler)
                anchor = toScene(QPointF(0.0, -EBTeachingMetrics::RulerHalfHeight));
            else if (_kind == Kind::Protractor)
                anchor = toScene(QPointF(0.0, -EBTeachingMetrics::ProtractorRadius));
            else
                anchor = toScene((_kind == Kind::Triangle45
                    ? EBTeachingMetrics::triangle45()
                    : EBTeachingMetrics::triangle30()).at(0));
            painter->drawLine(anchor, rotate);
            painter->drawEllipse(rotate, 7.0, 7.0);
        }
    }
    painter->restore();
}
