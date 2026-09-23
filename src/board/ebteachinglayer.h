#ifndef EBTEACHINGLAYER_H
#define EBTEACHINGLAYER_H

#include <QVector>

#include "ebteachingtools.h"

// 管理当前页可同时存在的多个教具，具体几何和绘制仍归各教具负责。
class EBTeachingLayer
{
public:
    using Kind = EBTeachingTools::Kind;

    void add(Kind kind, const QRectF &page);
    void deactivate();
    void removeActive();
    void clear();
    Kind kind() const;
    int count() const;
    QVector<EBTeachingState> states(const QRectF &page) const;
    void restore(const QVector<EBTeachingState> &states, const QRectF &page);
    bool press(const QPointF &position, const QRectF &page);
    void move(const QPointF &position, const QRectF &page);
    QPainterPath release(const QPointF &position, const QRectF &page);
    bool isInteracting() const;
    void cancel();
    QPainterPath circlePath() const;
    void flipActive(bool horizontal);
    void resetActive();
    void zoomMagnifier(qreal delta);
    void toggleMagnifierShape();
    void paint(QPainter *painter, QGraphicsScene *scene,
               const QRectF &page) const;

private:
    QVector<EBTeachingTools> _items;
    int _active = -1;
};

#endif
