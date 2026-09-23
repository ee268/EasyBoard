#include "ebobjectsnap.h"

#include <QtMath>

namespace {
qreal anchor(const QRectF &bounds, bool horizontal, int index)
{
    if (horizontal) {
        if (index == 0)
            return bounds.left();
        return index == 1 ? bounds.center().x() : bounds.right();
    }
    if (index == 0)
        return bounds.top();
    return index == 1 ? bounds.center().y() : bounds.bottom();
}

void consider(EBObjectSnap::Axis *axis, const QRectF &moving,
              const QRectF &reference, bool horizontal, bool page,
              qreal tolerance)
{
    for (int movingAnchor = 0; movingAnchor < 3; ++movingAnchor) {
        for (int referenceAnchor = 0; referenceAnchor < 3; ++referenceAnchor) {
            // 页面只按对应的左、中、右或上、中、下位置吸附。
            if (page && movingAnchor != referenceAnchor)
                continue;
            const qreal coordinate = anchor(reference, horizontal, referenceAnchor);
            const qreal offset = coordinate
                - anchor(moving, horizontal, movingAnchor);
            if (qAbs(offset) > tolerance
                || (axis->matched && qAbs(offset) >= qAbs(axis->offset)))
                continue;
            axis->matched = true;
            axis->offset = offset;
            axis->coordinate = coordinate;
            axis->reference = reference;
            axis->page = page;
        }
    }
}

void considerGrid(EBObjectSnap::Axis *axis, const QRectF &moving,
                  const QRectF &page, bool horizontal, qreal tolerance,
                  qreal spacing)
{
    if (axis->matched || spacing <= 0.0)
        return;
    const qreal origin = horizontal ? page.left() : page.top();
    const qreal limit = horizontal ? page.right() : page.bottom();
    for (int index = 0; index < 3; ++index) {
        const qreal position = anchor(moving, horizontal, index);
        const qreal coordinate = origin
            + qRound((position - origin) / spacing) * spacing;
        const qreal offset = coordinate - position;
        if (coordinate < origin || coordinate > limit
            || qAbs(offset) > tolerance
            || (axis->matched && qAbs(offset) >= qAbs(axis->offset)))
            continue;
        axis->matched = true;
        axis->offset = offset;
        axis->coordinate = coordinate;
        axis->reference = page;
        axis->grid = true;
    }
}

void considerGuides(EBObjectSnap::Axis *axis, const QRectF &moving,
                    const QRectF &page, const QVector<qreal> &guides,
                    bool horizontal, qreal tolerance)
{
    if (axis->matched)
        return;
    for (qreal guide : guides) {
        const qreal coordinate = (horizontal ? page.left() : page.top()) + guide;
        for (int index = 0; index < 3; ++index) {
            const qreal offset = coordinate - anchor(moving, horizontal, index);
            if (qAbs(offset) > tolerance
                || (axis->matched && qAbs(offset) >= qAbs(axis->offset)))
                continue;
            axis->matched = true;
            axis->offset = offset;
            axis->coordinate = coordinate;
            axis->reference = page;
            axis->manualGuide = true;
        }
    }
}
}

EBObjectSnap::Result EBObjectSnap::calculate(
    const QRectF &moving, const QRectF &page,
    const QVector<QRectF> &references, qreal tolerance, qreal gridSpacing,
    const QVector<qreal> &verticalGuides,
    const QVector<qreal> &horizontalGuides)
{
    Result result;
    if (moving.isEmpty() || page.isEmpty() || tolerance < 0.0)
        return result;
    consider(&result.horizontal, moving, page, true, true, tolerance);
    consider(&result.vertical, moving, page, false, true, tolerance);
    for (const QRectF &reference : references) {
        if (reference.isEmpty())
            continue;
        consider(&result.horizontal, moving, reference, true, false, tolerance);
        consider(&result.vertical, moving, reference, false, false, tolerance);
    }
    considerGuides(&result.horizontal, moving, page, verticalGuides,
                   true, tolerance);
    considerGuides(&result.vertical, moving, page, horizontalGuides,
                   false, tolerance);
    // 页面和其他对象优先；只有没有参照命中时才落到页面网格。
    considerGrid(&result.horizontal, moving, page, true, tolerance, gridSpacing);
    considerGrid(&result.vertical, moving, page, false, tolerance, gridSpacing);
    return result;
}
