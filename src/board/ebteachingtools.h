#ifndef EBTEACHINGTOOLS_H
#define EBTEACHINGTOOLS_H

#include <QPainterPath>
#include <QLineF>
#include <QPointF>
#include <QRectF>
#include <QVector>

#include "ebteachingstate.h"

class QGraphicsScene;
class QPainter;

// 教学教具是临时视图叠层，只有沿边描出的几何路径进入页面笔迹。
class EBTeachingTools
{
public:
    using Kind = EBTeachingState::Kind;

    void setKind(Kind kind, const QRectF &page);
    Kind kind() const;
    bool press(const QPointF &position, const QRectF &page,
               bool controlsVisible = true);
    void move(const QPointF &position, const QRectF &page);
    QPainterPath release(const QPointF &position, const QRectF &page);
    bool isInteracting() const;
    void cancel();
    QPainterPath circlePath() const;
    EBTeachingState state(const QRectF &page) const;
    void restore(const EBTeachingState &state, const QRectF &page);
    bool closeRequested() const;
    void flip(bool horizontal);
    void resetMeasurement();
    void zoomMagnifier(qreal delta);
    void toggleMagnifierShape();
    void paint(QPainter *painter, QGraphicsScene *scene,
               const QRectF &page, bool active) const;

private:
    enum class Operation { None, Move, Rotate, Resize, Draw, Measure, Close };
    QPainterPath toolShape() const;
    QVector<QLineF> edges() const;
    QPointF toLocal(const QPointF &position) const;
    QPointF toScene(const QPointF &position) const;
    QPointF constrained(const QPointF &position, const QRectF &page) const;
    QPainterPath drawingPath(const QPointF &position) const;
    qreal angleAt(const QPointF &position) const;
    QPointF closePoint() const;
    QPointF resizePoint() const;
    QPointF rotationPoint() const;
    QPointF compassPencilPoint() const;
    QPointF compassHingePoint() const;
    void paintRuler(QPainter *painter) const;
    void paintTriangle(QPainter *painter) const;
    void paintProtractor(QPainter *painter) const;
    void paintCompass(QPainter *painter) const;

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
    qreal _magnification = 2.0;
    bool _flipHorizontal = false;
    bool _flipVertical = false;
    bool _rectangularLens = false;
    bool _closeRequested = false;
};

#endif
