#ifndef EBBOARDVIEW_H
#define EBBOARDVIEW_H

#include <QGraphicsView>
#include <QFont>

#include "ebboardscene.h"
#include "ebobjectclipboard.h"

class QUndoStack;
class QKeyEvent;
class QPainter;
class QGraphicsLineItem;
class QRubberBand;
class EBDocument;

// 视图只负责视口坐标、鼠标操作会话和撤销入口；页面内容交给场景。
class EBBoardView : public QGraphicsView
{
    Q_OBJECT

public:
    enum class DrawingTool {
        Select,
        Text,
        Pen,
        Marker,
        Line,
        Eraser,
        Pointer,
        Pan,
        Rectangle,
        Ellipse,
        Arrow
    };
    using PageColor = EBBoardScene::PageColor;
    using PagePattern = EBBoardScene::PagePattern;
    using PageSize = EBBoardScene::PageSize;
    using ObjectArrangement = EBBoardScene::ObjectArrangement;

    explicit EBBoardView(EBDocument *document, QWidget *parent = nullptr);

    void setDrawingTool(DrawingTool tool);
    DrawingTool drawingTool() const;
    void setSnapEnabled(bool enabled);
    bool snapEnabled() const;
    void setGridSnapEnabled(bool enabled);
    bool gridSnapEnabled() const;
    QColor penColor() const;
    void setPenColor(const QColor &color);
    QColor markerColor() const;
    void setMarkerColor(const QColor &color);
    qreal penWidth() const;
    void setPenWidth(qreal width);
    qreal markerWidth() const;
    void setMarkerWidth(qreal width);
    bool canFormatSelectedText() const;
    QFont selectedTextFont() const;
    QColor selectedTextColor() const;
    void setSelectedTextFont(const QFont &font);
    void setSelectedTextColor(const QColor &color);
    bool canUndo() const;
    bool canRedo() const;
    void undo();
    void redo();
    bool hasSelectedObject() const;
    bool hasEditableSelectedObjects() const;
    bool canSelectAllObjects() const;
    void selectAllObjects();
    void scaleSelectedObject(qreal factor);
    void rotateSelectedObject(qreal degrees);
    void deleteSelectedObject();
    void copySelectedObject();
    void cutSelectedObject();
    void pasteObject();
    void duplicateSelectedObjects();
    bool canLockSelectedObjects() const;
    bool canUnlockSelectedObjects() const;
    void lockSelectedObjects();
    void unlockSelectedObjects();
    bool canPasteObject() const;
    bool isTextEditing() const;
    bool canMoveSelectedObjectBackward() const;
    bool canMoveSelectedObjectForward() const;
    bool canGroupSelectedObjects() const;
    bool canUngroupSelectedObjects() const;
    void groupSelectedObjects();
    void ungroupSelectedObjects();
    bool canArrangeSelectedObjects(ObjectArrangement arrangement) const;
    void arrangeSelectedObjects(ObjectArrangement arrangement);
    void sendSelectedObjectToBack();
    void moveSelectedObjectBackward();
    void moveSelectedObjectForward();
    void bringSelectedObjectToFront();
    bool insertImageObject(const EBImageItem::State &state);
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
    void selectionAvailabilityChanged(bool available);
    void drawingToolChanged(DrawingTool tool);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void drawForeground(QPainter *painter, const QRectF &rect) override;

private:
    using Snapshot = EBBoardScene::Snapshot;
    class PageEditCommand;
    enum class GuideAxis { None, Horizontal, Vertical };
    enum class TransformHandle { None, Scale, Rotate };
    struct TransformState {
        QGraphicsItem *item;
        QPointF position;
        QPointF center;
        qreal scale;
        qreal rotation;
    };

    void applyViewState();
    void zoomBy(qreal factor);
    QPointF boundedPagePosition(const QPointF &viewportPosition) const;
    void updateActiveStroke(const QPointF &pagePosition);
    void eraseAlong(const QPointF &from, const QPointF &to);
    void updateObjectMove(const QPointF &scenePosition, bool snapEnabled);
    void finishObjectMove();
    void hideSnapGuides();
    void refreshManualGuides();
    bool guideAt(const QPoint &viewportPosition, GuideAxis *axis,
                 int *index) const;
    void startGuideDrag(GuideAxis axis, int index, qreal value);
    void updateGuideDrag(const QPointF &scenePosition);
    void finishGuideDrag(bool commit);
    void removeGuide(GuideAxis axis, int index);
    QRectF selectedObjectBounds() const;
    QVector<QPoint> selectionFrame() const;
    TransformHandle transformHandleAt(const QPoint &viewportPosition) const;
    void drawSelectionHandles(QPainter *painter);
    void startObjectTransform(TransformHandle handle,
                              const QPointF &scenePosition);
    void updateObjectTransform(const QPointF &scenePosition);
    void finishObjectTransform();
    void startAreaSelection(const QPoint &viewportPosition,
                            Qt::KeyboardModifiers modifiers);
    void updateAreaSelection(const QPoint &viewportPosition);
    void finishAreaSelection();
    void nudgeSelectedObjects(const QPointF &delta);
    void finishKeyboardMove();
    void insertObjects(EBObjectClipboard::Objects objects, bool offsetObjects,
                       const QString &description);
    void startTextEditing(EBTextItem *text, bool newlyCreated);
    void finishTextEditing();
    void keepObjectInsidePage(QGraphicsItem *item);
    void keepObjectsInsidePage(const QVector<QGraphicsItem *> &items);
    void commitObjectEdit(const Snapshot &before, const QString &description);
    void moveSelectedObjectLayer(EBBoardScene::LayerMove move,
                                 const QString &description);
    void restoreSnapshot(const Snapshot &snapshot);
    void beginEdit();
    void finishEdit();
    void finishPageInteraction();
    void showCurrentPage();
    void syncCurrentPageStrokes();

    EBDocument *_document;
    EBBoardScene *_scene;
    EBStrokeItem *_activeStroke;
    QVector<QGraphicsItem *> _movingObjects;
    QRubberBand *_selectionBand;
    QPoint _selectionOrigin;
    QVector<QGraphicsItem *> _selectionBaseline;
    Qt::KeyboardModifiers _selectionModifiers;
    bool _selectingArea;
    bool _keyboardMoving;
    EBTextItem *_editingText;
    QString _textBeforeEdit;
    bool _editingTextWasNew;
    DrawingTool _drawingTool;
    DrawingTool _activeTool;
    QPointF _strokeStart;
    QPointF _moveStartScene;
    QVector<QPointF> _moveStartPositions;
    QVector<QRectF> _snapReferences;
    QGraphicsLineItem *_verticalGuide;
    QGraphicsLineItem *_horizontalGuide;
    QVector<QGraphicsLineItem *> _manualGuideItems;
    QGraphicsLineItem *_guidePreview;
    GuideAxis _guideAxis;
    int _guideIndex;
    qreal _guideValue;
    TransformHandle _transformHandle;
    QVector<TransformState> _transformStates;
    QRectF _transformBounds;
    QPointF _transformStartScene;
    QString _editDescription;
    bool _snapEnabled;
    bool _gridSnapEnabled;
    QColor _penColor;
    QColor _markerColor;
    qreal _penWidth;
    qreal _markerWidth;
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
