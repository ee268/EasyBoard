#ifndef EBBOARDVIEW_H
#define EBBOARDVIEW_H

#include <QGraphicsView>
#include <QRectF>

class QGraphicsScene;

class EBBoardView : public QGraphicsView
{
    Q_OBJECT
public:
    enum class DrawingTool {
        Pen,
        Marker,
        Line,
        Eraser,
        Pointer
    };

    explicit EBBoardView(QWidget *parent = nullptr);

    //切换下一笔使用的工具；已经开始的笔迹保持按下时选定的工具。
    void setDrawingTool(DrawingTool tool);
    DrawingTool drawingTool() const;

    //窗口内坐标映射到白板坐标
    QPointF toPagePosition(const QPointF& viewportPos) const;
    QRectF pageRect() const;

protected:
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    void fitPage();

    // 拖动离开页面时将末端停在页边，避免笔迹画进灰色工作台。
    QPointF boundedPagePosition(const QPoint &viewportPosition) const;

    // 自由笔迹追加采样点，直线则只保留起点和当前终点。
    void updateActiveStroke(const QPointF &pagePosition);

    // 橡皮只移除接触位置附近的路径段，两侧笔迹保留为独立片段。
    void eraseAt(const QPointF &pagePosition);
    void eraseAlong(const QPointF &from, const QPointF &to);

    // 指示点是临时图元，不参与笔迹保存和橡皮命中。
    void movePointerTo(const QPointF &pagePosition);

    QGraphicsScene* _scene;
    QRectF _pageRect;

    QGraphicsPathItem* _activeStroke;

    DrawingTool _drawingTool;
    DrawingTool _activeTool;
    QPointF _strokeStart;

    bool _erasing;
    QPointF _lastEraserPosition;
    bool _pointing;
    QGraphicsEllipseItem *_pointerItem;

signals:
    //白板内坐标对于鼠标移动的映射
    void pagePositionChanged(const QPointF &pagePosition, bool insidePage);
};

#endif // EBBOARDVIEW_H
