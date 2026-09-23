#include "ebteachingtools.h"

#include <QtMath>

void EBTeachingTools::setKind(Kind kind, const QRectF &page)
{
    _kind = kind;
    _operation = Operation::None;
    _center = page.center();
    _rotation = 0.0;
    _scale = 1.0;
    _radius = qMin(300.0, page.width() / 3.0);
    _startAngle = 0.0;
    _sweepAngle = 0.0;
    _measuredAngle = 0.0;
    _edgeIndex = -1;
    _magnification = 2.0;
    _flipHorizontal = false;
    _flipVertical = false;
    _rectangularLens = false;
    _closeRequested = false;
    _frame = QRectF(_center - QPointF(140.0, 90.0), QSizeF(280.0, 180.0));
    if (kind == Kind::Curtain)
        _frame = QRectF(_center - QPointF(page.width() / 4.0,
                                          page.height() / 4.0),
                        QSizeF(page.width() / 2.0, page.height() / 2.0));
    else if (kind == Kind::Spotlight || kind == Kind::Magnifier)
        _frame = QRectF(_center - QPointF(110.0, 75.0), QSizeF(220.0, 150.0));
}

EBTeachingTools::Kind EBTeachingTools::kind() const
{
    return _kind;
}
