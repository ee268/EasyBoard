#ifndef EBOBJECTSNAP_H
#define EBOBJECTSNAP_H

#include <QRectF>
#include <QVector>

// 根据移动对象整体边界计算水平、垂直吸附；不修改场景或图元。
class EBObjectSnap
{
public:
    struct Axis {
        bool matched = false;
        qreal offset = 0.0;
        qreal coordinate = 0.0;
        QRectF reference;
        bool page = false;
        bool grid = false;
        bool manualGuide = false;
    };
    struct Result {
        Axis horizontal;
        Axis vertical;
    };

    static Result calculate(const QRectF &moving, const QRectF &page,
                            const QVector<QRectF> &references, qreal tolerance,
                            qreal gridSpacing = 0.0,
                            const QVector<qreal> &verticalGuides = {},
                            const QVector<qreal> &horizontalGuides = {});
};

#endif
