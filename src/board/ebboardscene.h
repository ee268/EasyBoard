#ifndef EBBOARDSCENE_H
#define EBBOARDSCENE_H

#include <QGraphicsScene>
#include <QRectF>
#include "../domain/ebpage.h"

class QGraphicsEllipseItem;
class QGraphicsRectItem;

// 场景显示当前页面的背景、临时指示点和全部笔迹图元。
class EBBoardScene : public QGraphicsScene
{
public:
    using PageColor = EBPage::Color;
    using PagePattern = EBPage::Pattern;
    using Snapshot = EBPage::Strokes;

    explicit EBBoardScene(QObject *parent = nullptr);

    QRectF pageRect() const;
    void showPage(const EBPage &page);
    void setPageColor(PageColor color);
    PageColor pageColor() const;
    void setPagePattern(PagePattern pattern);
    PagePattern pagePattern() const;

    // 视图只报告操作意图，图元的创建、裁切和恢复统一由场景管理。
    EBStrokeItem *addStroke(const QPainterPath &path, const QPen &pen);
    bool eraseAt(const QPointF &pagePosition, qreal radius);
    Snapshot captureStrokes() const;
    void restoreStrokes(const Snapshot &snapshot);
    void showPointerAt(const QPointF &pagePosition);
    void hidePointer();
    bool pointerVisible() const;

private:
    void refreshPageBackground();

    QRectF _pageRect;
    QGraphicsRectItem *_pageItem;
    QGraphicsEllipseItem *_pointerItem;
    PageColor _pageColor;
    PagePattern _pagePattern;
};

#endif
