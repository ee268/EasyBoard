#ifndef EBBOARDVIEW_H
#define EBBOARDVIEW_H

#include <QGraphicsView>

#include "ebboardscene.h"

class QUndoStack;
class EBDocument;

// 视图只负责视口坐标、鼠标操作会话和撤销入口；页面内容交给场景。
class EBBoardView : public QGraphicsView
{
    Q_OBJECT

public:
    enum class DrawingTool {
        Pen,
        Marker,
        Line,
        Eraser,
        Pointer,
        Pan
    };
    using PageColor = EBBoardScene::PageColor;
    using PagePattern = EBBoardScene::PagePattern;
    using PageSize = EBBoardScene::PageSize;

    explicit EBBoardView(EBDocument *document, QWidget *parent = nullptr);

    void setDrawingTool(DrawingTool tool);
    DrawingTool drawingTool() const;
    bool canUndo() const;
    bool canRedo() const;
    void undo();
    void redo();
    int addPage();
    int duplicateCurrentPage();
    bool removeCurrentPage();
    bool moveCurrentPage(int offset);
    bool setCurrentPageIndex(int index);
    void commitCurrentPage();
    void reloadDocument();

    // 缩放倍数相对于“适应页面”；平移中心保存在场景坐标中。
    void zoomIn();
    void zoomOut();
    void fitPage();
    qreal zoomFactor() const;

    // 保持原接口供主窗口使用，背景状态同步到文档当前页。
    void setPageColor(PageColor color);
    PageColor pageColor() const;
    void setPagePattern(PagePattern pattern);
    PagePattern pagePattern() const;
    void setPageSize(PageSize size);
    PageSize pageSize() const;
    QRectF pageRect() const;
    QPointF toPagePosition(const QPointF &viewportPosition) const;

signals:
    void pagePositionChanged(const QPointF &pagePosition, bool insidePage);
    void historyAvailabilityChanged(bool undoAvailable, bool redoAvailable);
    void currentPageChanged(int index);
    void pageContentChanged(int index);
    void pageListChanged();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    using Snapshot = EBBoardScene::Snapshot;
    class StrokeEditCommand;

    void applyViewState();
    void zoomBy(qreal factor);
    QPointF boundedPagePosition(const QPointF &viewportPosition) const;
    void updateActiveStroke(const QPointF &pagePosition);
    void eraseAlong(const QPointF &from, const QPointF &to);
    void beginEdit();
    void finishEdit();
    void finishPageInteraction();
    void showCurrentPage();
    void syncCurrentPageStrokes();

    EBDocument *_document;
    EBBoardScene *_scene;
    EBStrokeItem *_activeStroke;
    DrawingTool _drawingTool;
    DrawingTool _activeTool;
    QPointF _strokeStart;
    bool _erasing;
    QPointF _lastEraserPosition;
    bool _pointing;
    QUndoStack *_undoStack;
    Snapshot _editBefore;
    bool _editActive;
    bool _editChanged;
    qreal _zoomFactor;
    QPointF _viewCenter;
    bool _viewStateReady;
};

#endif
