#include "ebboardview.h"

#include <QApplication>
#include <QBuffer>
#include <QClipboard>
#include <QImage>
#include <QMimeData>
#include <QScopedPointer>
#include <QUuid>
#include <QtMath>

#include "../global/ebtheme.h"
#include "ebobjectclipboard.h"

namespace {
constexpr qreal kMinObjectScale = 0.25;
constexpr qreal kMaxObjectScale = 4.0;
constexpr qreal kPasteOffset = 24.0;
}

bool EBBoardView::hasSelectedObject() const
{
    return !_editingText && !_scene->selectedObjects().isEmpty();
}

bool EBBoardView::hasEditableSelectedObjects() const
{
    return !_editingText && _scene->selectedObjectsEditable();
}

bool EBBoardView::canSelectAllObjects() const
{
    return !_editingText && _scene->hasObjects();
}

void EBBoardView::selectAllObjects()
{
    if (_editingText)
        return;
    finishObjectMove();
    setDrawingTool(DrawingTool::Select);
    _scene->selectAllObjects();
}

void EBBoardView::scaleSelectedObject(qreal factor)
{
    finishTextEditing();
    finishObjectMove();
    const QVector<QGraphicsItem *> objects = _scene->selectedObjects();
    if (!_scene->selectedObjectsEditable() || factor <= 0.0)
        return;
    const Snapshot before = _scene->captureSnapshot();
    bool changed = false;
    for (QGraphicsItem *object : objects) {
        const qreal scale = qBound(kMinObjectScale, object->scale() * factor,
                                   kMaxObjectScale);
        changed |= !qFuzzyCompare(scale, object->scale());
        object->setScale(scale);
    }
    if (!changed)
        return;
    keepObjectsInsidePage(objects);
    commitObjectEdit(before, factor > 1.0 ? tr("放大对象") : tr("缩小对象"));
}

void EBBoardView::rotateSelectedObject(qreal degrees)
{
    finishTextEditing();
    finishObjectMove();
    const QVector<QGraphicsItem *> objects = _scene->selectedObjects();
    if (!_scene->selectedObjectsEditable() || qFuzzyIsNull(degrees))
        return;
    const Snapshot before = _scene->captureSnapshot();
    for (QGraphicsItem *object : objects)
        object->setRotation(object->rotation() + degrees);
    keepObjectsInsidePage(objects);
    commitObjectEdit(before, degrees > 0.0 ? tr("顺时针旋转对象")
                                           : tr("逆时针旋转对象"));
}

void EBBoardView::deleteSelectedObject()
{
    finishTextEditing();
    finishObjectMove();
    const QVector<QGraphicsItem *> objects = _scene->selectedObjects();
    if (!_scene->selectedObjectsEditable())
        return;
    const Snapshot before = _scene->captureSnapshot();
    for (QGraphicsItem *object : objects)
        delete object;
    commitObjectEdit(before, tr("删除对象"));
}

void EBBoardView::copySelectedObject()
{
    if (_editingText)
        return;
    finishObjectMove();
    if (QMimeData *mimeData = EBObjectClipboard::createMimeData(
            _scene->selectedObjects()))
        QApplication::clipboard()->setMimeData(mimeData);
}

void EBBoardView::cutSelectedObject()
{
    if (!hasEditableSelectedObjects())
        return;
    copySelectedObject();
    deleteSelectedObject();
}

bool EBBoardView::canPasteObject() const
{
    if (_editingText)
        return false;
    const QMimeData *mimeData = QApplication::clipboard()->mimeData();
    return EBObjectClipboard::canDecode(mimeData)
        || mimeData->hasFormat(QStringLiteral("image/svg+xml"))
        || mimeData->hasImage() || mimeData->hasText();
}

bool EBBoardView::isTextEditing() const
{
    return _editingText != nullptr;
}

void EBBoardView::pasteObject()
{
    if (_editingText)
        return;
    const QMimeData *mimeData = QApplication::clipboard()->mimeData();
    EBObjectClipboard::Objects objects;
    const bool internal = EBObjectClipboard::decode(mimeData, &objects);
    EBObjectClipboard::Object object;

    if (!internal && mimeData->hasFormat(QStringLiteral("image/svg+xml"))) {
        object.type = EBObjectClipboard::Type::Image;
        object.image.format = EBImageItem::Format::Svg;
        object.image.data = mimeData->data(QStringLiteral("image/svg+xml"));
        if (!EBImageItem::naturalSize(object.image.format, object.image.data,
                                      &object.image.size))
            object.type = EBObjectClipboard::Type::Invalid;
    } else if (!internal && mimeData->hasImage()) {
        const QImage image = qvariant_cast<QImage>(mimeData->imageData());
        QByteArray data;
        QBuffer buffer(&data);
        if (!image.isNull() && buffer.open(QIODevice::WriteOnly)
            && image.save(&buffer, "PNG")) {
            object.type = EBObjectClipboard::Type::Image;
            object.image.format = EBImageItem::Format::Png;
            object.image.data = data;
            object.image.size = image.size();
        }
    } else if (!internal && mimeData->hasText()) {
        const QString text = mimeData->text();
        if (!text.isEmpty()) {
            object.type = EBObjectClipboard::Type::Text;
            object.text.text = text.left(100000);
            object.text.font.setPointSize(22);
            object.text.color = ebThemeColor(EBThemeColor::BoardPen);
        }
    }
    if (!internal) {
        if (object.type == EBObjectClipboard::Type::Invalid)
            return;
        objects.append(object);
    } else {
        // 副本使用新的组合标识，避免粘贴后与原对象联动。
        EBObjectClipboard::remapGroupIds(&objects);
    }

    insertObjects(objects, internal, tr("粘贴对象"));
}

void EBBoardView::duplicateSelectedObjects()
{
    if (_editingText)
        return;
    finishObjectMove();
    QScopedPointer<QMimeData> mimeData(
        EBObjectClipboard::createMimeData(_scene->selectedObjects()));
    EBObjectClipboard::Objects objects;
    if (!mimeData || !EBObjectClipboard::decode(mimeData.data(), &objects))
        return;
    EBObjectClipboard::remapGroupIds(&objects);
    insertObjects(objects, true, tr("复制对象"));
}

bool EBBoardView::canLockSelectedObjects() const
{
    return !_editingText && _scene->canLockSelectedObjects();
}

bool EBBoardView::canUnlockSelectedObjects() const
{
    return !_editingText && _scene->canUnlockSelectedObjects();
}

void EBBoardView::lockSelectedObjects()
{
    finishTextEditing();
    finishObjectMove();
    if (!_scene->canLockSelectedObjects())
        return;
    const Snapshot before = _scene->captureSnapshot();
    if (_scene->setSelectedObjectsLocked(true))
        commitObjectEdit(before, tr("锁定对象"));
}

void EBBoardView::unlockSelectedObjects()
{
    finishTextEditing();
    finishObjectMove();
    if (!_scene->canUnlockSelectedObjects())
        return;
    const Snapshot before = _scene->captureSnapshot();
    if (_scene->setSelectedObjectsLocked(false))
        commitObjectEdit(before, tr("解锁对象"));
}

void EBBoardView::insertObjects(EBObjectClipboard::Objects objects,
                                bool offsetObjects,
                                const QString &description)
{
    if (objects.isEmpty())
        return;
    finishPageInteraction();
    setDrawingTool(DrawingTool::Select);
    const Snapshot before = _scene->captureSnapshot();
    qreal pastedZValue = _scene->nextObjectZValue();
    _scene->clearSelection();
    QVector<QGraphicsItem *> pastedItems;
    for (EBObjectClipboard::Object &entry : objects) {
        QGraphicsItem *item = nullptr;
        if (entry.type == EBObjectClipboard::Type::Stroke) {
            if (offsetObjects)
                entry.stroke.position += QPointF(kPasteOffset, kPasteOffset);
            entry.stroke.zValue = pastedZValue++;
            EBStrokeItem *stroke = _scene->addStroke(entry.stroke.path,
                                                     entry.stroke.pen);
            stroke->applyState(entry.stroke);
            item = stroke;
        } else if (entry.type == EBObjectClipboard::Type::Text) {
            EBTextItem *text = _scene->addText(entry.text.text,
                                               entry.text.font,
                                               entry.text.color);
            if (offsetObjects) {
                entry.text.position += QPointF(kPasteOffset, kPasteOffset);
                entry.text.zValue = pastedZValue++;
                text->applyState(entry.text);
            } else {
                text->setPos(pageRect().center()
                             - QPointF(text->boundingRect().width() / 2.0,
                                       text->boundingRect().height() / 2.0));
                text->setZValue(pastedZValue++);
                text->refreshTransformOrigin();
            }
            item = text;
        } else if (entry.type == EBObjectClipboard::Type::Image) {
            if (offsetObjects) {
                entry.image.position += QPointF(kPasteOffset, kPasteOffset);
            } else {
                const qreal fit = qMin(1.0, qMin(
                    pageRect().width() * 0.6 / entry.image.size.width(),
                    pageRect().height() * 0.6 / entry.image.size.height()));
                entry.image.size *= fit;
                entry.image.position = pageRect().center()
                    - QPointF(entry.image.size.width() / 2.0,
                              entry.image.size.height() / 2.0);
                entry.image.transformOrigin = QPointF(
                    entry.image.size.width() / 2.0,
                    entry.image.size.height() / 2.0);
            }
            entry.image.zValue = pastedZValue++;
            item = _scene->addImage(entry.image);
        }
        if (!item) {
            restoreSnapshot(before);
            return;
        }
        item->setSelected(true);
        pastedItems.append(item);
    }
    keepObjectsInsidePage(pastedItems);
    commitObjectEdit(before, description);
}

bool EBBoardView::canMoveSelectedObjectBackward() const
{
    return !_editingText && _scene->canMoveSelectedObjectBackward();
}

bool EBBoardView::canMoveSelectedObjectForward() const
{
    return !_editingText && _scene->canMoveSelectedObjectForward();
}

bool EBBoardView::canGroupSelectedObjects() const
{
    return !_editingText && _scene->canGroupSelectedObjects();
}

bool EBBoardView::canUngroupSelectedObjects() const
{
    return !_editingText && _scene->canUngroupSelectedObjects();
}

void EBBoardView::groupSelectedObjects()
{
    finishTextEditing();
    finishObjectMove();
    if (!_scene->canGroupSelectedObjects())
        return;
    const Snapshot before = _scene->captureSnapshot();
    const QString groupId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (_scene->groupSelectedObjects(groupId))
        commitObjectEdit(before, tr("组合对象"));
}

void EBBoardView::ungroupSelectedObjects()
{
    finishTextEditing();
    finishObjectMove();
    if (!_scene->canUngroupSelectedObjects())
        return;
    const Snapshot before = _scene->captureSnapshot();
    if (_scene->ungroupSelectedObjects())
        commitObjectEdit(before, tr("取消组合"));
}

bool EBBoardView::canArrangeSelectedObjects(ObjectArrangement arrangement) const
{
    return !_editingText && _scene->canArrangeSelectedObjects(arrangement);
}

void EBBoardView::arrangeSelectedObjects(ObjectArrangement arrangement)
{
    finishTextEditing();
    finishObjectMove();
    if (!_scene->canArrangeSelectedObjects(arrangement))
        return;
    const Snapshot before = _scene->captureSnapshot();
    if (!_scene->arrangeSelectedObjects(arrangement))
        return;
    QString description;
    switch (arrangement) {
    case ObjectArrangement::AlignLeft:
        description = tr("对象左对齐");
        break;
    case ObjectArrangement::AlignHorizontalCenter:
        description = tr("对象水平居中");
        break;
    case ObjectArrangement::AlignRight:
        description = tr("对象右对齐");
        break;
    case ObjectArrangement::AlignTop:
        description = tr("对象顶部对齐");
        break;
    case ObjectArrangement::AlignVerticalCenter:
        description = tr("对象垂直居中");
        break;
    case ObjectArrangement::AlignBottom:
        description = tr("对象底部对齐");
        break;
    case ObjectArrangement::DistributeHorizontal:
        description = tr("对象水平等距分布");
        break;
    case ObjectArrangement::DistributeVertical:
        description = tr("对象垂直等距分布");
        break;
    }
    commitObjectEdit(before, description);
}

void EBBoardView::sendSelectedObjectToBack()
{
    moveSelectedObjectLayer(EBBoardScene::LayerMove::ToBack,
                            tr("对象置于底层"));
}

void EBBoardView::moveSelectedObjectBackward()
{
    moveSelectedObjectLayer(EBBoardScene::LayerMove::Backward,
                            tr("对象下移一层"));
}

void EBBoardView::moveSelectedObjectForward()
{
    moveSelectedObjectLayer(EBBoardScene::LayerMove::Forward,
                            tr("对象上移一层"));
}

void EBBoardView::bringSelectedObjectToFront()
{
    moveSelectedObjectLayer(EBBoardScene::LayerMove::ToFront,
                            tr("对象置于顶层"));
}

bool EBBoardView::insertImageObject(const EBImageItem::State &source)
{
    QSizeF naturalSize;
    if (!EBImageItem::naturalSize(source.format, source.data, &naturalSize))
        return false;
    finishPageInteraction();
    setDrawingTool(DrawingTool::Select);
    const Snapshot before = _scene->captureSnapshot();
    EBImageItem::State state = source;
    if (!state.size.isValid() || state.size.isEmpty())
        state.size = naturalSize;
    const qreal fit = qMin(1.0, qMin(pageRect().width() * 0.6 / state.size.width(),
                                     pageRect().height() * 0.6 / state.size.height()));
    state.size *= fit;
    state.position = pageRect().center()
                     - QPointF(state.size.width() / 2.0,
                               state.size.height() / 2.0);
    state.zValue = _scene->nextObjectZValue();
    state.transformOrigin = QPointF(state.size.width() / 2.0,
                                    state.size.height() / 2.0);
    state.scale = 1.0;
    state.rotation = 0.0;
    EBImageItem *image = _scene->addImage(state);
    if (!image)
        return false;
    _scene->clearSelection();
    image->setSelected(true);
    commitObjectEdit(before, tr("插入图片"));
    return true;
}


