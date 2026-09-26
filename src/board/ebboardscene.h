#ifndef EBBOARDSCENE_H
#define EBBOARDSCENE_H

#include <QGraphicsScene>
#include <QRectF>
#include "../domain/ebpage.h"

class QGraphicsEllipseItem;
class QGraphicsPixmapItem;
class QGraphicsRectItem;

// 场景显示当前页面的背景、临时指示点和全部笔迹图元。
class EBBoardScene : public QGraphicsScene
{
public:
    static constexpr int PatternSpacing = 40;
    enum class LayerMove {
        ToBack,
        Backward,
        Forward,
        ToFront
    };
    enum class ObjectArrangement {
        AlignLeft,
        AlignHorizontalCenter,
        AlignRight,
        AlignTop,
        AlignVerticalCenter,
        AlignBottom,
        DistributeHorizontal,
        DistributeVertical
    };
    using PageColor = EBPage::Color;
    using PagePattern = EBPage::Pattern;
    using PageSize = EBPage::Size;
    using StrokeSnapshot = EBPage::Strokes;
    struct Snapshot {
        EBPage::Strokes strokes;
        EBPage::Texts texts;
        EBPage::Images images;
        QVector<qreal> horizontalGuides;
        QVector<qreal> verticalGuides;
    };

    explicit EBBoardScene(QObject *parent = nullptr);

    QRectF pageRect() const;
    void showPage(const EBPage &page);
    void setPageColor(PageColor color);
    PageColor pageColor() const;
    void setPagePattern(PagePattern pattern);
    PagePattern pagePattern() const;
    void setPageSize(PageSize size, qreal customWidth = 1200.0,
                     qreal customHeight = EBPage::Height);
    PageSize pageSize() const;
    const QVector<qreal> &horizontalGuides() const;
    const QVector<qreal> &verticalGuides() const;
    void setGuides(const QVector<qreal> &horizontal,
                   const QVector<qreal> &vertical);

    // 视图只报告操作意图，图元的创建、裁切和恢复统一由场景管理。
    EBStrokeItem *addStroke(const QPainterPath &path, const QPen &pen);
    EBTextItem *addText(const QString &text, const QFont &font,
                        const QColor &color);
    EBImageItem *addImage(const EBImageItem::State &state);
    bool eraseAt(const QPointF &pagePosition, qreal radius);
    StrokeSnapshot captureStrokes() const;
    void restoreStrokes(const StrokeSnapshot &snapshot);
    EBPage::Texts captureTexts() const;
    void restoreTexts(const EBPage::Texts &texts);
    EBPage::Images captureImages() const;
    void restoreImages(const EBPage::Images &images);
    Snapshot captureSnapshot() const;
    void restoreSnapshot(const Snapshot &snapshot);
    EBStrokeItem *strokeAt(const QPointF &scenePosition,
                           qreal tolerance = 6.0) const;
    EBTextItem *textAt(const QPointF &scenePosition) const;
    QGraphicsItem *objectAt(const QPointF &scenePosition,
                            qreal tolerance = 6.0) const;
    EBStrokeItem *selectedStroke() const;
    QGraphicsItem *selectedObject() const;
    QVector<QGraphicsItem *> selectedObjects() const;
    QVector<QGraphicsItem *> objectsInRect(const QRectF &sceneRect) const;
    bool hasObjects() const;
    QVector<QRectF> objectReferenceBounds() const;
    void selectAllObjects();
    bool isObjectLocked(QGraphicsItem *object) const;
    bool selectedObjectsEditable() const;
    bool canLockSelectedObjects() const;
    bool canUnlockSelectedObjects() const;
    bool setSelectedObjectsLocked(bool locked);
    void setObjectSelected(QGraphicsItem *object, bool selected);
    bool canGroupSelectedObjects() const;
    bool canUngroupSelectedObjects() const;
    bool groupSelectedObjects(const QString &groupId);
    bool ungroupSelectedObjects();
    bool canArrangeSelectedObjects(ObjectArrangement arrangement) const;
    bool arrangeSelectedObjects(ObjectArrangement arrangement);
    qreal nextObjectZValue() const;
    bool canMoveSelectedObjectBackward() const;
    bool canMoveSelectedObjectForward() const;
    bool moveSelectedObject(LayerMove move);
    void setObjectInteractionEnabled(bool enabled);
    bool objectInteractionEnabled() const;
    void showPointerAt(const QPointF &pagePosition);
    void hidePointer();
    bool pointerVisible() const;

private:
    QVector<QGraphicsItem *> objectItems() const;
    QString objectGroupId(QGraphicsItem *object) const;
    void setObjectGroupId(QGraphicsItem *object, const QString &groupId);
    void setObjectLocked(QGraphicsItem *object, bool locked);
    QVector<QVector<QGraphicsItem *>> selectedObjectUnits() const;
    void refreshPageGeometry();
    void refreshPageBackground();
    void refreshPageImage();

    QRectF _pageRect;
    QGraphicsRectItem *_pageItem;
    QGraphicsPixmapItem *_pageImageItem;
    QGraphicsEllipseItem *_pointerItem;
    PageColor _pageColor;
    PagePattern _pagePattern;
    PageSize _pageSize;
    qreal _customPageWidth;
    qreal _customPageHeight;
    QImage _pageBackgroundImage;
    QVector<qreal> _horizontalGuides;
    QVector<qreal> _verticalGuides;
    bool _objectInteractionEnabled;
};

#endif
