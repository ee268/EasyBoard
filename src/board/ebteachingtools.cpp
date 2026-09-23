#include "ebteachingtools.h"

#include <QGraphicsScene>
#include <QImage>
#include <QPainter>
#include <QTransform>
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

QPen teachingPen(const QColor &color, qreal width = 1.5)
{
    QPen pen(color, width);
    pen.setCosmetic(true);
    return pen;
}
}

void EBTeachingTools::setKind(Kind kind, const QRectF &page)
{
    _kind = kind;
    _operation = Operation::None;
    _center = page.center();
    _rotation = 0.0;
    _scale = 1.0;
    _radius = qMin(100.0, qMin(page.width(), page.height()) / 4.0);
    _startAngle = 0.0;
    _sweepAngle = 0.0;
    _measuredAngle = 0.0;
    _edgeIndex = -1;
    _frame = QRectF(_center - QPointF(140.0, 90.0), QSizeF(280.0, 180.0));
    if (kind == Kind::Curtain)
        _frame = page.adjusted(60.0, 60.0, -60.0, -60.0);
    else if (kind == Kind::Spotlight || kind == Kind::Magnifier)
        _frame = QRectF(_center - QPointF(110.0, 75.0), QSizeF(220.0, 150.0));
}

EBTeachingTools::Kind EBTeachingTools::kind() const
{
    return _kind;
}

QPainterPath EBTeachingTools::toolShape() const
{
    QPainterPath shape;
    if (_kind == Kind::Ruler) {
        shape.addRoundedRect(QRectF(-160.0, -24.0, 320.0, 48.0), 4.0, 4.0);
    } else if (_kind == Kind::Triangle45) {
        shape.moveTo(-135.0, 80.0);
        shape.lineTo(135.0, 80.0);
        shape.lineTo(-135.0, -80.0);
        shape.closeSubpath();
    } else if (_kind == Kind::Triangle30) {
        shape.moveTo(-145.0, 75.0);
        shape.lineTo(145.0, 75.0);
        shape.lineTo(0.0, -75.0);
        shape.closeSubpath();
    } else if (_kind == Kind::Protractor) {
        shape.moveTo(-130.0, 0.0);
        shape.arcTo(QRectF(-130.0, -130.0, 260.0, 260.0), 180.0, -180.0);
        shape.closeSubpath();
    }
    return shape;
}

QVector<QLineF> EBTeachingTools::edges() const
{
    if (_kind == Kind::Ruler)
        return {QLineF(-160.0, -24.0, 160.0, -24.0),
                QLineF(-160.0, 24.0, 160.0, 24.0)};
    if (_kind == Kind::Triangle45)
        return {QLineF(-135.0, 80.0, 135.0, 80.0),
                QLineF(135.0, 80.0, -135.0, -80.0),
                QLineF(-135.0, -80.0, -135.0, 80.0)};
    if (_kind == Kind::Triangle30)
        return {QLineF(-145.0, 75.0, 145.0, 75.0),
                QLineF(145.0, 75.0, 0.0, -75.0),
                QLineF(0.0, -75.0, -145.0, 75.0)};
    return {};
}

QPointF EBTeachingTools::toLocal(const QPointF &position) const
{
    QTransform transform;
    transform.translate(_center.x(), _center.y());
    transform.rotate(_rotation);
    transform.scale(_scale, _scale);
    return transform.inverted().map(position);
}

QPointF EBTeachingTools::toScene(const QPointF &position) const
{
    QTransform transform;
    transform.translate(_center.x(), _center.y());
    transform.rotate(_rotation);
    transform.scale(_scale, _scale);
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

bool EBTeachingTools::press(const QPointF &position, const QRectF &page)
{
    if (_kind == Kind::None || !page.contains(position))
        return false;
    _lastPosition = position;
    if (_kind == Kind::Curtain || _kind == Kind::Spotlight
        || _kind == Kind::Magnifier) {
        const QPointF corner = _frame.bottomRight();
        if (QLineF(position, corner).length() <= 18.0) {
            _operation = Operation::Resize;
            return true;
        }
        if (_frame.contains(position)) {
            _operation = Operation::Move;
            return true;
        }
        return false;
    }
    if (_kind == Kind::Compass) {
        if (QLineF(position, _center + QPointF(_radius, 0.0)).length() <= 18.0) {
            _operation = Operation::Resize;
        } else if (QLineF(position, _center).length() <= 18.0) {
            _operation = Operation::Move;
        } else if (qAbs(QLineF(position, _center).length() - _radius)
                   <= kHitDistance) {
            _operation = Operation::Draw;
            _startAngle = angleAt(position);
            _lastAngle = _startAngle;
            _sweepAngle = 0.0;
        } else {
            return false;
        }
        return true;
    }
    const QPointF local = toLocal(position);
    const qreal rotationY = _kind == Kind::Protractor ? -160.0 : -110.0;
    if (QLineF(local, QPointF(0.0, rotationY)).length() <= 17.0) {
        _operation = Operation::Rotate;
        return true;
    }
    if (_kind == Kind::Protractor) {
        if (!toolShape().contains(local))
            return false;
        if (QLineF(local, QPointF()).length() > 80.0) {
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
            _radius = qBound(25.0, QLineF(_center, bounded).length(),
                             qMin(page.width(), page.height()) / 2.0);
        else
            _frame.setBottomRight(QPointF(
                qBound(_frame.left() + 80.0, bounded.x(), page.right()),
                qBound(_frame.top() + 60.0, bounded.y(), page.bottom())));
    } else if (_operation == Operation::Rotate) {
        _rotation = qRadiansToDegrees(qAtan2(bounded.y() - _center.y(),
                                            bounded.x() - _center.x())) + 90.0;
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
    _operation = Operation::None;
    _edgeIndex = -1;
    return path;
}

bool EBTeachingTools::isInteracting() const
{
    return _operation != Operation::None;
}

QPainterPath EBTeachingTools::circlePath() const
{
    QPainterPath path;
    if (_kind == Kind::Compass)
        path.addEllipse(_center, _radius, _radius);
    return path;
}

void EBTeachingTools::paint(QPainter *painter, QGraphicsScene *scene,
                            const QRectF &page) const
{
    if (_kind == Kind::None)
        return;
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    const QColor outline(46, 105, 117);
    const QColor fill(193, 229, 239, 180);
    if (_kind == Kind::Curtain) {
        painter->setPen(teachingPen(outline, 2.0));
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
        painter->setPen(teachingPen(outline, 2.0));
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(_frame);
    } else if (_kind == Kind::Magnifier) {
        const QRectF sample(_frame.center().x() - _frame.width() / 4.0,
                            _frame.center().y() - _frame.height() / 4.0,
                            _frame.width() / 2.0, _frame.height() / 2.0);
        const QSize imageSize(qMax(1, qCeil(_frame.width() * 2.0)),
                              qMax(1, qCeil(_frame.height() * 2.0)));
        QImage image(imageSize, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::white);
        QPainter imagePainter(&image);
        scene->render(&imagePainter, QRectF(QPointF(), imageSize), sample,
                      Qt::IgnoreAspectRatio);
        imagePainter.end();
        QPainterPath lens;
        lens.addEllipse(_frame);
        painter->setClipPath(lens);
        painter->drawImage(_frame, image);
        painter->setClipping(false);
        painter->setPen(teachingPen(outline, 3.0));
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(_frame);
    } else if (_kind == Kind::Compass) {
        painter->setPen(teachingPen(outline, 1.5));
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(_center, _radius, _radius);
        painter->drawLine(_center, _center + QPointF(_radius, 0.0));
        painter->setBrush(fill);
        painter->drawEllipse(_center, 9.0, 9.0);
        painter->drawEllipse(_center + QPointF(_radius, 0.0), 9.0, 9.0);
    } else {
        painter->save();
        painter->translate(_center);
        painter->rotate(_rotation);
        painter->scale(_scale, _scale);
        painter->setPen(teachingPen(outline, 2.0));
        painter->setBrush(fill);
        painter->drawPath(toolShape());
        if (_kind == Kind::Ruler) {
            for (int x = -150; x <= 150; x += 10) {
                const int length = x % 50 == 0 ? 14 : 7;
                painter->drawLine(x, -24, x, -24 + length);
            }
        } else if (_kind == Kind::Protractor) {
            for (int degree = 0; degree <= 180; degree += 10) {
                const qreal radians = qDegreesToRadians(qreal(degree));
                painter->drawLine(QPointF(118.0 * qCos(radians),
                                          -118.0 * qSin(radians)),
                                  QPointF(130.0 * qCos(radians),
                                          -130.0 * qSin(radians)));
            }
            painter->drawLine(QPointF(),
                QPointF(100.0 * qCos(qDegreesToRadians(_measuredAngle)),
                        -100.0 * qSin(qDegreesToRadians(_measuredAngle))));
            painter->drawText(QPointF(-15.0, -12.0),
                              QString::number(qRound(_measuredAngle))
                                  + QChar(0x00B0));
        }
        const qreal rotationY = _kind == Kind::Protractor ? -160.0 : -110.0;
        painter->drawLine(QPointF(0.0, rotationY + 10.0),
                          QPointF(0.0, rotationY + 35.0));
        painter->setBrush(Qt::white);
        painter->drawEllipse(QPointF(0.0, rotationY), 8.0, 8.0);
        painter->restore();
    }
    if ((_kind == Kind::Curtain || _kind == Kind::Spotlight
         || _kind == Kind::Magnifier)) {
        painter->setPen(teachingPen(outline, 2.0));
        painter->setBrush(Qt::white);
        painter->drawRect(QRectF(_frame.bottomRight() - QPointF(7.0, 7.0),
                                 QSizeF(14.0, 14.0)));
    }
    if (_operation == Operation::Draw) {
        painter->setPen(teachingPen(QColor(22, 114, 155), 2.0));
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(drawingPath(_lastPosition));
    }
    painter->restore();
}
