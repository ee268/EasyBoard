#include "ebboardscene.h"

#include <QGraphicsEllipseItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsRectItem>
#include <QImage>
#include <QHash>
#include <QPainter>
#include <QPixmap>
#include <QSet>
#include <QtMath>

#include <algorithm>

#include "../global/ebtheme.h"

namespace {
// 页面尺寸和边距属于场景；视图只负责把视口坐标换算为页面坐标。
constexpr qreal kPageMargin = 80.0;
constexpr qreal kPointerRadius = 7.0;
constexpr qreal kFirstObjectZValue = 1.0;
constexpr qreal kPointerZValue = 1000000.0;
}

EBBoardScene::EBBoardScene(QObject *parent)
    : QGraphicsScene(parent)
    , _pageRect(kPageMargin, kPageMargin,
                EBPage::widthForSize(PageSize::Standard), EBPage::Height)
    , _pageItem(nullptr)
    , _pageImageItem(nullptr)
    , _pointerItem(nullptr)
    , _pageColor(PageColor::White)
    , _pagePattern(PagePattern::Blank)
    , _pageSize(PageSize::Standard)
    , _objectInteractionEnabled(false)
{
    setSceneRect(0.0, 0.0,
                 EBPage::widthForSize(PageSize::Standard) + 2.0 * kPageMargin,
                 EBPage::Height + 2.0 * kPageMargin);
    _pageItem = addRect(_pageRect, QPen(ebThemeColor(EBThemeColor::BoardBorder)),
                         QBrush(ebThemeColor(EBThemeColor::BoardWhite)));
    refreshPageBackground();

    // 导入图片位于页面底纹之上、笔迹之下，不参与擦除和撤销。
    _pageImageItem = addPixmap(QPixmap());
    _pageImageItem->setZValue(0.5);
    _pageImageItem->hide();

    // 红点是临时指示图元，不属于持久笔迹和撤销快照。
    _pointerItem = addEllipse(-kPointerRadius, -kPointerRadius,
                              2.0 * kPointerRadius, 2.0 * kPointerRadius,
                              QPen(ebThemeColor(EBThemeColor::BoardPointerBorder), 2.0),
                              QBrush(ebThemeColor(EBThemeColor::BoardPointerFill)));
    _pointerItem->setZValue(kPointerZValue);
    _pointerItem->hide();
}

QRectF EBBoardScene::pageRect() const
{
    return _pageRect;
}

void EBBoardScene::showPage(const EBPage &page)
{
    hidePointer();
    setPageSize(page.size());
    setPageColor(page.color());
    setPagePattern(page.pattern());
    _pageBackgroundImage = page.backgroundImage();
    refreshPageImage();
    restoreSnapshot({page.strokes(), page.texts(), page.images(),
                     page.horizontalGuides(), page.verticalGuides()});
}

void EBBoardScene::setPageColor(PageColor color)
{
    if (_pageColor == color)
        return;
    _pageColor = color;
    refreshPageBackground();
}

EBBoardScene::PageColor EBBoardScene::pageColor() const
{
    return _pageColor;
}

void EBBoardScene::setPagePattern(PagePattern pattern)
{
    if (_pagePattern == pattern)
        return;
    _pagePattern = pattern;
    refreshPageBackground();
}

EBBoardScene::PagePattern EBBoardScene::pagePattern() const
{
    return _pagePattern;
}

void EBBoardScene::setPageSize(PageSize size)
{
    if (_pageSize == size)
        return;
    _pageSize = size;
    refreshPageGeometry();
}

EBBoardScene::PageSize EBBoardScene::pageSize() const
{
    return _pageSize;
}

const QVector<qreal> &EBBoardScene::horizontalGuides() const
{
    return _horizontalGuides;
}

const QVector<qreal> &EBBoardScene::verticalGuides() const
{
    return _verticalGuides;
}

void EBBoardScene::setGuides(const QVector<qreal> &horizontal,
                             const QVector<qreal> &vertical)
{
    _horizontalGuides = horizontal;
    _verticalGuides = vertical;
}

EBStrokeItem *EBBoardScene::addStroke(const QPainterPath &path, const QPen &pen)
{
    EBStrokeItem *stroke = new EBStrokeItem(path, pen);
    stroke->setFlag(QGraphicsItem::ItemIsSelectable,
                    _objectInteractionEnabled);
    addItem(stroke);
    stroke->setPos(_pageRect.topLeft());
    stroke->setZValue(nextObjectZValue());
    return stroke;
}

EBTextItem *EBBoardScene::addText(const QString &text, const QFont &font,
                                  const QColor &color)
{
    EBTextItem *item = new EBTextItem(text, font, color);
    item->setFlag(QGraphicsItem::ItemIsSelectable,
                  _objectInteractionEnabled);
    item->setTextInteractionFlags(Qt::NoTextInteraction);
    addItem(item);
    item->setZValue(nextObjectZValue());
    return item;
}

EBImageItem *EBBoardScene::addImage(const EBImageItem::State &state)
{
    EBImageItem *item = new EBImageItem(state);
    if (!item->isValid()) {
        delete item;
        return nullptr;
    }
    item->setFlag(QGraphicsItem::ItemIsSelectable,
                  _objectInteractionEnabled);
    addItem(item);
    return item;
}

bool EBBoardScene::eraseAt(const QPointF &pagePosition, qreal radius)
{
    const QPointF scenePosition = pagePosition + _pageRect.topLeft();
    const QRectF touchArea(scenePosition.x() - radius, scenePosition.y() - radius,
                           2.0 * radius, 2.0 * radius);
    bool changed = false;
    // 只裁切自有笔迹图元；页面背景与指示点不会被橡皮影响。
    for (QGraphicsItem *item : items(touchArea, Qt::IntersectsItemShape)) {
        auto *stroke = dynamic_cast<EBStrokeItem *>(item);
        if (!stroke || stroke->isLocked())
            continue;
        QVector<QPainterPath> remaining;
        const qreal cutRadius = radius + stroke->pen().widthF() / 2.0;
        if (!stroke->splitAt(stroke->mapFromScene(scenePosition), cutRadius, remaining))
            continue;

        changed = true;
        const EBStrokeItem::State original = stroke->state();
        delete stroke;
        for (const QPainterPath &part : remaining) {
            EBStrokeItem *kept = addStroke(part, original.pen);
            EBStrokeItem::State state = original;
            state.path = part;
            if (qFuzzyCompare(original.scale, 1.0)
                && qFuzzyIsNull(original.rotation))
                state.transformOrigin = part.boundingRect().center();
            kept->applyState(state);
        }
    }
    return changed;
}

EBBoardScene::StrokeSnapshot EBBoardScene::captureStrokes() const
{
    StrokeSnapshot snapshot;
    // 从底到顶保存，以便撤销命令重建时保留笔迹的覆盖顺序。
    for (QGraphicsItem *item : items(Qt::AscendingOrder)) {
        if (auto *stroke = dynamic_cast<EBStrokeItem *>(item))
            snapshot.append(stroke->state());
    }
    return snapshot;
}

void EBBoardScene::restoreStrokes(const StrokeSnapshot &snapshot)
{
    for (QGraphicsItem *item : items()) {
        if (auto *stroke = dynamic_cast<EBStrokeItem *>(item))
            delete stroke;
    }
    for (const EBStrokeItem::State &state : snapshot) {
        EBStrokeItem *stroke = addStroke(state.path, state.pen);
        stroke->applyState(state);
    }
}

EBPage::Texts EBBoardScene::captureTexts() const
{
    EBPage::Texts texts;
    for (QGraphicsItem *item : items(Qt::AscendingOrder)) {
        if (auto *text = dynamic_cast<EBTextItem *>(item))
            texts.append(text->state());
    }
    return texts;
}

void EBBoardScene::restoreTexts(const EBPage::Texts &texts)
{
    for (QGraphicsItem *item : items()) {
        if (auto *text = dynamic_cast<EBTextItem *>(item))
            delete text;
    }
    for (const EBTextItem::State &state : texts) {
        EBTextItem *text = addText(state.text, state.font, state.color);
        text->applyState(state);
    }
}

EBPage::Images EBBoardScene::captureImages() const
{
    EBPage::Images images;
    for (QGraphicsItem *item : items(Qt::AscendingOrder)) {
        if (auto *image = dynamic_cast<EBImageItem *>(item))
            images.append(image->state());
    }
    return images;
}

void EBBoardScene::restoreImages(const EBPage::Images &images)
{
    for (QGraphicsItem *item : items()) {
        if (auto *image = dynamic_cast<EBImageItem *>(item))
            delete image;
    }
    for (const EBImageItem::State &state : images)
        addImage(state);
}

EBBoardScene::Snapshot EBBoardScene::captureSnapshot() const
{
    return {captureStrokes(), captureTexts(), captureImages(),
            _horizontalGuides, _verticalGuides};
}

void EBBoardScene::restoreSnapshot(const Snapshot &snapshot)
{
    restoreStrokes(snapshot.strokes);
    restoreTexts(snapshot.texts);
    restoreImages(snapshot.images);
    setGuides(snapshot.horizontalGuides, snapshot.verticalGuides);
}

void EBBoardScene::showPointerAt(const QPointF &pagePosition)
{
    _pointerItem->setPos(pagePosition + _pageRect.topLeft());
    _pointerItem->show();
}

void EBBoardScene::hidePointer()
{
    _pointerItem->hide();
}

bool EBBoardScene::pointerVisible() const
{
    return _pointerItem->isVisible();
}

EBStrokeItem *EBBoardScene::strokeAt(const QPointF &scenePosition,
                                     qreal tolerance) const
{
    QPainterPath hitArea;
    hitArea.addEllipse(scenePosition, tolerance, tolerance);
    for (QGraphicsItem *item : items(hitArea, Qt::IntersectsItemShape,
                                     Qt::DescendingOrder)) {
        if (auto *stroke = dynamic_cast<EBStrokeItem *>(item))
            return stroke;
    }
    return nullptr;
}

EBTextItem *EBBoardScene::textAt(const QPointF &scenePosition) const
{
    for (QGraphicsItem *item : items(scenePosition, Qt::IntersectsItemShape,
                                     Qt::DescendingOrder)) {
        if (auto *text = dynamic_cast<EBTextItem *>(item))
            return text;
    }
    return nullptr;
}

QGraphicsItem *EBBoardScene::objectAt(const QPointF &scenePosition,
                                      qreal tolerance) const
{
    QPainterPath hitArea;
    hitArea.addEllipse(scenePosition, tolerance, tolerance);
    for (QGraphicsItem *item : items(hitArea, Qt::IntersectsItemShape,
                                     Qt::DescendingOrder)) {
        if (dynamic_cast<EBStrokeItem *>(item)
            || dynamic_cast<EBTextItem *>(item)
            || dynamic_cast<EBImageItem *>(item))
            return item;
    }
    return nullptr;
}

EBStrokeItem *EBBoardScene::selectedStroke() const
{
    for (QGraphicsItem *item : selectedItems()) {
        if (auto *stroke = dynamic_cast<EBStrokeItem *>(item))
            return stroke;
    }
    return nullptr;
}

QGraphicsItem *EBBoardScene::selectedObject() const
{
    const QVector<QGraphicsItem *> objects = selectedObjects();
    return objects.isEmpty() ? nullptr : objects.constLast();
}

QVector<QGraphicsItem *> EBBoardScene::selectedObjects() const
{
    QVector<QGraphicsItem *> selected;
    for (QGraphicsItem *item : objectItems()) {
        if (item->isSelected())
            selected.append(item);
    }
    return selected;
}

QVector<QGraphicsItem *> EBBoardScene::objectsInRect(
    const QRectF &sceneRect) const
{
    QSet<QGraphicsItem *> hits;
    QSet<QString> groupIds;
    for (QGraphicsItem *item : items(sceneRect, Qt::IntersectsItemShape,
                                     Qt::AscendingOrder)) {
        if (dynamic_cast<EBStrokeItem *>(item)
            || dynamic_cast<EBTextItem *>(item)
            || dynamic_cast<EBImageItem *>(item)) {
            hits.insert(item);
            const QString groupId = objectGroupId(item);
            if (!groupId.isEmpty())
                groupIds.insert(groupId);
        }
    }

    QVector<QGraphicsItem *> result;
    for (QGraphicsItem *object : objectItems()) {
        if (hits.contains(object)
            || groupIds.contains(objectGroupId(object)))
            result.append(object);
    }
    return result;
}

bool EBBoardScene::hasObjects() const
{
    return !objectItems().isEmpty();
}

QVector<QRectF> EBBoardScene::objectReferenceBounds() const
{
    QVector<QRectF> bounds;
    QHash<QString, int> groupedIndexes;
    for (QGraphicsItem *object : objectItems()) {
        if (object->isSelected())
            continue;
        const QString groupId = objectGroupId(object);
        const QRectF itemBounds = object->sceneBoundingRect();
        if (groupId.isEmpty()) {
            bounds.append(itemBounds);
            continue;
        }
        const auto found = groupedIndexes.constFind(groupId);
        if (found == groupedIndexes.constEnd()) {
            groupedIndexes.insert(groupId, bounds.size());
            bounds.append(itemBounds);
        } else {
            bounds[found.value()] = bounds.at(found.value()).united(itemBounds);
        }
    }
    return bounds;
}

void EBBoardScene::selectAllObjects()
{
    for (QGraphicsItem *object : objectItems())
        object->setSelected(true);
}

bool EBBoardScene::isObjectLocked(QGraphicsItem *object) const
{
    if (auto *stroke = dynamic_cast<EBStrokeItem *>(object))
        return stroke->isLocked();
    if (auto *text = dynamic_cast<EBTextItem *>(object))
        return text->isLocked();
    if (auto *image = dynamic_cast<EBImageItem *>(object))
        return image->isLocked();
    return false;
}

bool EBBoardScene::selectedObjectsEditable() const
{
    const QVector<QGraphicsItem *> selected = selectedObjects();
    if (selected.isEmpty())
        return false;
    for (QGraphicsItem *object : selected) {
        if (isObjectLocked(object))
            return false;
    }
    return true;
}

bool EBBoardScene::canLockSelectedObjects() const
{
    for (QGraphicsItem *object : selectedObjects()) {
        if (!isObjectLocked(object))
            return true;
    }
    return false;
}

bool EBBoardScene::canUnlockSelectedObjects() const
{
    for (QGraphicsItem *object : selectedObjects()) {
        if (isObjectLocked(object))
            return true;
    }
    return false;
}

bool EBBoardScene::setSelectedObjectsLocked(bool locked)
{
    if (locked ? !canLockSelectedObjects() : !canUnlockSelectedObjects())
        return false;
    for (QGraphicsItem *object : selectedObjects())
        setObjectLocked(object, locked);
    return true;
}

void EBBoardScene::setObjectSelected(QGraphicsItem *object, bool selected)
{
    if (!object)
        return;
    const QString groupId = objectGroupId(object);
    if (groupId.isEmpty()) {
        object->setSelected(selected);
        return;
    }
    for (QGraphicsItem *candidate : objectItems()) {
        if (objectGroupId(candidate) == groupId)
            candidate->setSelected(selected);
    }
}

bool EBBoardScene::canGroupSelectedObjects() const
{
    if (!selectedObjectsEditable())
        return false;
    const QVector<QGraphicsItem *> selected = selectedObjects();
    if (selected.size() < 2)
        return false;
    const QString groupId = objectGroupId(selected.first());
    if (groupId.isEmpty())
        return true;
    for (QGraphicsItem *object : selected) {
        if (objectGroupId(object) != groupId)
            return true;
    }
    return false;
}

bool EBBoardScene::canUngroupSelectedObjects() const
{
    if (!selectedObjectsEditable())
        return false;
    for (QGraphicsItem *object : selectedObjects()) {
        if (!objectGroupId(object).isEmpty())
            return true;
    }
    return false;
}

bool EBBoardScene::groupSelectedObjects(const QString &groupId)
{
    if (groupId.isEmpty() || !canGroupSelectedObjects())
        return false;
    for (QGraphicsItem *object : selectedObjects())
        setObjectGroupId(object, groupId);
    return true;
}

bool EBBoardScene::ungroupSelectedObjects()
{
    if (!canUngroupSelectedObjects())
        return false;
    for (QGraphicsItem *object : selectedObjects())
        setObjectGroupId(object, QString());
    return true;
}

bool EBBoardScene::canArrangeSelectedObjects(
    ObjectArrangement arrangement) const
{
    if (!selectedObjectsEditable())
        return false;
    const int unitCount = selectedObjectUnits().size();
    const bool distribution = arrangement == ObjectArrangement::DistributeHorizontal
        || arrangement == ObjectArrangement::DistributeVertical;
    return unitCount >= (distribution ? 3 : 2);
}

bool EBBoardScene::arrangeSelectedObjects(ObjectArrangement arrangement)
{
    QVector<QVector<QGraphicsItem *>> units = selectedObjectUnits();
    if (!canArrangeSelectedObjects(arrangement))
        return false;

    QVector<QRectF> bounds;
    QRectF selectionBounds;
    for (const QVector<QGraphicsItem *> &unit : units) {
        QRectF unitBounds = unit.first()->sceneBoundingRect();
        for (int index = 1; index < unit.size(); ++index)
            unitBounds = unitBounds.united(unit.at(index)->sceneBoundingRect());
        bounds.append(unitBounds);
        selectionBounds = bounds.size() == 1
            ? unitBounds : selectionBounds.united(unitBounds);
    }

    QVector<QPointF> offsets(units.size());
    if (arrangement == ObjectArrangement::AlignLeft
        || arrangement == ObjectArrangement::AlignHorizontalCenter
        || arrangement == ObjectArrangement::AlignRight) {
        for (int index = 0; index < units.size(); ++index) {
            qreal x = selectionBounds.left() - bounds.at(index).left();
            if (arrangement == ObjectArrangement::AlignHorizontalCenter)
                x = selectionBounds.center().x() - bounds.at(index).center().x();
            else if (arrangement == ObjectArrangement::AlignRight)
                x = selectionBounds.right() - bounds.at(index).right();
            offsets[index].setX(x);
        }
    } else if (arrangement == ObjectArrangement::AlignTop
               || arrangement == ObjectArrangement::AlignVerticalCenter
               || arrangement == ObjectArrangement::AlignBottom) {
        for (int index = 0; index < units.size(); ++index) {
            qreal y = selectionBounds.top() - bounds.at(index).top();
            if (arrangement == ObjectArrangement::AlignVerticalCenter)
                y = selectionBounds.center().y() - bounds.at(index).center().y();
            else if (arrangement == ObjectArrangement::AlignBottom)
                y = selectionBounds.bottom() - bounds.at(index).bottom();
            offsets[index].setY(y);
        }
    } else {
        QVector<int> order;
        order.reserve(units.size());
        for (int index = 0; index < units.size(); ++index)
            order.append(index);
        const bool horizontal = arrangement
            == ObjectArrangement::DistributeHorizontal;
        std::sort(order.begin(), order.end(), [&bounds, horizontal](int left,
                                                                    int right) {
            return horizontal
                ? bounds.at(left).center().x() < bounds.at(right).center().x()
                : bounds.at(left).center().y() < bounds.at(right).center().y();
        });
        qreal totalSize = 0.0;
        for (int index : order)
            totalSize += horizontal ? bounds.at(index).width()
                                    : bounds.at(index).height();
        const qreal span = horizontal ? selectionBounds.width()
                                      : selectionBounds.height();
        const qreal gap = (span - totalSize) / (units.size() - 1);
        qreal cursor = horizontal ? selectionBounds.left()
                                  : selectionBounds.top();
        for (int index : order) {
            if (horizontal) {
                offsets[index].setX(cursor - bounds.at(index).left());
                cursor += bounds.at(index).width() + gap;
            } else {
                offsets[index].setY(cursor - bounds.at(index).top());
                cursor += bounds.at(index).height() + gap;
            }
        }
    }

    bool changed = false;
    for (int index = 0; index < units.size(); ++index) {
        const QPointF offset = offsets.at(index);
        if (qFuzzyIsNull(offset.x()) && qFuzzyIsNull(offset.y()))
            continue;
        changed = true;
        for (QGraphicsItem *object : units.at(index))
            object->moveBy(offset.x(), offset.y());
    }
    return changed;
}

qreal EBBoardScene::nextObjectZValue() const
{
    qreal value = kFirstObjectZValue;
    for (QGraphicsItem *item : objectItems())
        value = qMax(value, item->zValue() + 1.0);
    return value;
}

bool EBBoardScene::canMoveSelectedObjectBackward() const
{
    if (!selectedObjectsEditable())
        return false;
    const QVector<QGraphicsItem *> objects = objectItems();
    const QVector<QGraphicsItem *> selected = selectedObjects();
    if (selected.isEmpty())
        return false;
    for (QGraphicsItem *item : objects) {
        if (!item->isSelected())
            return true;
        if (selected.contains(item))
            break;
    }
    return false;
}

bool EBBoardScene::canMoveSelectedObjectForward() const
{
    if (!selectedObjectsEditable())
        return false;
    const QVector<QGraphicsItem *> objects = objectItems();
    const QVector<QGraphicsItem *> selected = selectedObjects();
    if (selected.isEmpty())
        return false;
    for (int index = objects.size() - 1; index >= 0; --index) {
        if (!objects.at(index)->isSelected())
            return true;
        if (selected.contains(objects.at(index)))
            break;
    }
    return false;
}

bool EBBoardScene::moveSelectedObject(LayerMove move)
{
    if (!selectedObjectsEditable())
        return false;
    QVector<QGraphicsItem *> objects = objectItems();
    const QVector<QGraphicsItem *> selected = selectedObjects();
    if (selected.isEmpty())
        return false;
    const QVector<QGraphicsItem *> before = objects;
    if (move == LayerMove::ToBack || move == LayerMove::ToFront) {
        QVector<QGraphicsItem *> unselected;
        for (QGraphicsItem *item : objects) {
            if (!item->isSelected())
                unselected.append(item);
        }
        objects = move == LayerMove::ToBack
            ? selected + unselected : unselected + selected;
    } else if (move == LayerMove::Backward) {
        for (int index = 1; index < objects.size(); ++index) {
            if (objects.at(index)->isSelected()
                && !objects.at(index - 1)->isSelected())
                objects.swapItemsAt(index, index - 1);
        }
    } else {
        for (int index = objects.size() - 2; index >= 0; --index) {
            if (objects.at(index)->isSelected()
                && !objects.at(index + 1)->isSelected())
                objects.swapItemsAt(index, index + 1);
        }
    }
    bool changed = objects.size() != before.size();
    for (int index = 0; !changed && index < objects.size(); ++index)
        changed = objects.at(index) != before.at(index);
    if (!changed)
        return false;
    // 使用连续且唯一的层级值，避免旧对象相同 z 值导致恢复后次序含糊。
    for (int index = 0; index < objects.size(); ++index)
        objects.at(index)->setZValue(kFirstObjectZValue + index);
    return true;
}

void EBBoardScene::setObjectInteractionEnabled(bool enabled)
{
    if (_objectInteractionEnabled == enabled)
        return;
    _objectInteractionEnabled = enabled;
    for (QGraphicsItem *item : items()) {
        if (dynamic_cast<EBStrokeItem *>(item)
            || dynamic_cast<EBTextItem *>(item)
            || dynamic_cast<EBImageItem *>(item))
            item->setFlag(QGraphicsItem::ItemIsSelectable, enabled);
    }
    if (!enabled)
        clearSelection();
}

bool EBBoardScene::objectInteractionEnabled() const
{
    return _objectInteractionEnabled;
}

QVector<QGraphicsItem *> EBBoardScene::objectItems() const
{
    QVector<QGraphicsItem *> objects;
    for (QGraphicsItem *item : items(Qt::AscendingOrder)) {
        if (dynamic_cast<EBStrokeItem *>(item)
            || dynamic_cast<EBTextItem *>(item)
            || dynamic_cast<EBImageItem *>(item))
            objects.append(item);
    }
    return objects;
}

QString EBBoardScene::objectGroupId(QGraphicsItem *object) const
{
    if (auto *stroke = dynamic_cast<EBStrokeItem *>(object))
        return stroke->groupId();
    if (auto *text = dynamic_cast<EBTextItem *>(object))
        return text->groupId();
    if (auto *image = dynamic_cast<EBImageItem *>(object))
        return image->groupId();
    return QString();
}

void EBBoardScene::setObjectGroupId(QGraphicsItem *object,
                                    const QString &groupId)
{
    if (auto *stroke = dynamic_cast<EBStrokeItem *>(object))
        stroke->setGroupId(groupId);
    else if (auto *text = dynamic_cast<EBTextItem *>(object))
        text->setGroupId(groupId);
    else if (auto *image = dynamic_cast<EBImageItem *>(object))
        image->setGroupId(groupId);
}

void EBBoardScene::setObjectLocked(QGraphicsItem *object, bool locked)
{
    if (auto *stroke = dynamic_cast<EBStrokeItem *>(object))
        stroke->setLocked(locked);
    else if (auto *text = dynamic_cast<EBTextItem *>(object))
        text->setLocked(locked);
    else if (auto *image = dynamic_cast<EBImageItem *>(object))
        image->setLocked(locked);
}

QVector<QVector<QGraphicsItem *>> EBBoardScene::selectedObjectUnits() const
{
    QVector<QVector<QGraphicsItem *>> units;
    QHash<QString, int> groupedIndexes;
    for (QGraphicsItem *object : selectedObjects()) {
        const QString groupId = objectGroupId(object);
        if (groupId.isEmpty()) {
            units.append({object});
            continue;
        }
        auto found = groupedIndexes.constFind(groupId);
        if (found == groupedIndexes.constEnd()) {
            groupedIndexes.insert(groupId, units.size());
            units.append({object});
        } else {
            units[found.value()].append(object);
        }
    }
    return units;
}

void EBBoardScene::refreshPageGeometry()
{
    const qreal width = EBPage::widthForSize(_pageSize);
    _pageRect = QRectF(kPageMargin, kPageMargin, width, EBPage::Height);
    _pageItem->setRect(_pageRect);
    setSceneRect(0.0, 0.0, width + 2.0 * kPageMargin,
                 EBPage::Height + 2.0 * kPageMargin);
    refreshPageImage();
}

void EBBoardScene::refreshPageBackground()
{
    // 各页按自身选择读取主题色；缩略图场景不修改全局主题。
    const QColor base = _pageColor == PageColor::White
        ? ebThemeColor(EBThemeColor::BoardWhite)
        : ebThemeColor(EBThemeColor::BoardCream);
    if (_pagePattern == PagePattern::Blank) {
        _pageItem->setBrush(QBrush(base));
        return;
    }

    // 底纹由代码绘制，保持现有 EasyBoard 页面效果。
    QImage tile(PatternSpacing, PatternSpacing, QImage::Format_ARGB32_Premultiplied);
    tile.fill(base);
    QPainter painter(&tile);
    const QColor patternColor = _pageColor == PageColor::White
        ? ebThemeColor(EBThemeColor::BoardWhitePattern)
        : ebThemeColor(EBThemeColor::BoardCreamPattern);
    painter.setPen(QPen(patternColor, 1.0));
    if (_pagePattern == PagePattern::Grid) {
        painter.drawLine(0, 0, PatternSpacing - 1, 0);
        painter.drawLine(0, 0, 0, PatternSpacing - 1);
    } else {
        painter.drawLine(0, PatternSpacing - 1,
                         PatternSpacing - 1, PatternSpacing - 1);
    }
    painter.end();
    _pageItem->setBrush(QBrush(QPixmap::fromImage(tile)));
}

void EBBoardScene::refreshPageImage()
{
    if (!_pageImageItem || _pageBackgroundImage.isNull()) {
        if (_pageImageItem)
            _pageImageItem->hide();
        return;
    }

    const QImage scaled = _pageBackgroundImage.scaled(
        _pageRect.size().toSize(), Qt::KeepAspectRatio,
        Qt::SmoothTransformation);
    _pageImageItem->setPixmap(QPixmap::fromImage(scaled));
    _pageImageItem->setPos(
        _pageRect.left() + (_pageRect.width() - scaled.width()) / 2.0,
        _pageRect.top() + (_pageRect.height() - scaled.height()) / 2.0);
    _pageImageItem->show();
}
