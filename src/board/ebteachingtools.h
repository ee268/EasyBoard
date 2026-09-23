#ifndef EBTEACHINGTOOLS_H
#define EBTEACHINGTOOLS_H

#include <QPainterPath>
#include <QLineF>
#include <QPointF>
#include <QRectF>
#include <QVector>

class QGraphicsScene;
class QPainter;

// 教学教具是临时视图叠层，只有沿边描出的几何路径进入页面笔迹。
class EBTeachingTools
{
public:
    enum class Kind {
        None, Ruler, Triangle45, Triangle30, Protractor,
        Compass, Curtain, Spotlight, Magnifier
    };

    void setKind(Kind kind, const QRectF &page);
    Kind kind() const;
    bool press(const QPointF &position, const QRectF &page);
    void move(const QPointF &position, const QRectF &page);
    QPainterPath release(const QPointF &position, const QRectF &page);
    bool isInteracting() const;
    QPainterPath circlePath() const;
    void paint(QPainter *painter, QGraphicsScene *scene,
               const QRectF &page) const;

private:
    enum class Operation { None, Move, Rotate, Resize, Draw, Measure };
    QPainterPath toolShape() const;
    QVector<QLineF> edges() const;
    QPointF toLocal(const QPointF &position) const;
    QPointF toScene(const QPointF &position) const;
    QPointF constrained(const QPointF &position, const QRectF &page) const;
    QPainterPath drawingPath(const QPointF &position) const;
    qreal angleAt(const QPointF &position) const;

    Kind _kind = Kind::None;
    Operation _operation = Operation::None;
    QPointF _center;
    QPointF _lastPosition;
    QPointF _drawStart;
    QPointF _drawEnd;
    QRectF _frame;
    qreal _rotation = 0.0;
    qreal _scale = 1.0;
    qreal _radius = 100.0;
    qreal _startAngle = 0.0;
    qreal _lastAngle = 0.0;
    qreal _sweepAngle = 0.0;
    qreal _measuredAngle = 0.0;
    int _edgeIndex = -1;
};

#endif
