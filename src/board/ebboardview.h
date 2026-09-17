#ifndef EBBOARDVIEW_H
#define EBBOARDVIEW_H

#include <QGraphicsView>
#include <QRectF>

class QGraphicsScene;

class EBBoardView : public QGraphicsView
{
    Q_OBJECT
public:
    explicit EBBoardView(QWidget *parent = nullptr);

    //窗口内坐标映射到白板坐标
    QPointF toPagePosition(const QPointF& viewportPos) const;
    QRectF pageRect() const;

protected:
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    void fitPage();

    // 拖动离开页面时将末端停在页边，避免笔迹画进灰色工作台。
    QPointF boundedPagePosition(const QPoint &viewportPosition) const;

    QGraphicsScene* _scene;
    QRectF _pageRect;

    QGraphicsPathItem* _activeStroke;

signals:
    //白板内坐标对于鼠标移动的映射
    void pagePositionChanged(const QPointF &pagePosition, bool insidePage);
};

#endif // EBBOARDVIEW_H
