#include "ebboardview.h"

#include <QHideEvent>
#include <QApplication>
#include <QBuffer>
#include <QClipboard>
#include <QKeyEvent>
#include <QLineF>
#include <QMouseEvent>
#include <QPainter>
#include <QMimeData>
#include <QResizeEvent>
#include <QRubberBand>
#include <QScopedPointer>
#include <QSet>
#include <QShowEvent>
#include <QTextCursor>
#include <QUndoCommand>
#include <QUndoStack>
#include <QUuid>
#include <QWheelEvent>
#include <QtMath>

#include "../global/ebtheme.h"
#include "../domain/ebdocument.h"
#include "ebobjectclipboard.h"

namespace {
constexpr qreal kPenWidth = 3.0;
constexpr qreal kMarkerWidth = 18.0;
constexpr qreal kEraserRadius = 12.0;
constexpr int kMaxHistoryEntries = 50;
constexpr qreal kZoomStep = 1.25;
constexpr qreal kMinZoomFactor = 0.5;
constexpr qreal kMaxZoomFactor = 4.0;
constexpr qreal kMinObjectScale = 0.25;
constexpr qreal kMaxObjectScale = 4.0;
constexpr qreal kPasteOffset = 24.0;
}

// 一次绘制、文字编辑或对象操作是一项撤销命令；场景负责恢复页面对象。
class EBBoardView::PageEditCommand : public QUndoCommand
{
public:
    PageEditCommand(EBBoardView *view, const Snapshot &before,
                      const Snapshot &after, const QString &description)
        : QUndoCommand(description)
        , _view(view)
        , _before(before)
        , _after(after)
        , _firstRedo(true)
    {
    }

    void undo() override
    {
        _view->restoreSnapshot(_before);
    }

    void redo() override
    {
        // 入栈时 Qt 会立即调用 redo；实时绘制已把场景置于操作后状态。
        if (_firstRedo) {
            _firstRedo = false;
            return;
        }
        _view->restoreSnapshot(_after);
    }

private:
    EBBoardView *_view;
    Snapshot _before;
    Snapshot _after;
    bool _firstRedo;
};

EBBoardView::EBBoardView(EBDocument *document, QWidget *parent)
    : QGraphicsView(parent)
    , _document(document)
    , _scene(new EBBoardScene(this))
    , _activeStroke(nullptr)
    , _selectionBand(new QRubberBand(QRubberBand::Rectangle, viewport()))
    , _selectionModifiers(Qt::NoModifier)
    , _selectingArea(false)
    , _keyboardMoving(false)
    , _editingText(nullptr)
    , _editingTextWasNew(false)
    , _drawingTool(DrawingTool::Pen)
    , _activeTool(DrawingTool::Pen)
    , _erasing(false)
    , _pointing(false)
    , _undoStack(new QUndoStack(this))
    , _editActive(false)
    , _editChanged(false)
    , _zoomFactor(1.0)
    , _viewCenter(_scene->sceneRect().center())
    , _viewStateReady(false)
{
    Q_ASSERT(_document);
    _scene->showPage(*_document->currentPage());
    setObjectName(QStringLiteral("boardView"));
    _undoStack->setObjectName(QStringLiteral("boardUndoStack"));
    _undoStack->setUndoLimit(kMaxHistoryEntries);
    setScene(_scene);
    setBackgroundBrush(ebThemeColor(EBThemeColor::BoardBackground));
    setRenderHint(QPainter::Antialiasing);
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setMouseTracking(true);
    viewport()->setMouseTracking(true);
    connect(_scene, &QGraphicsScene::selectionChanged, this, [this]() {
        emit selectionAvailabilityChanged(hasSelectedObject());
    });
    connect(_scene, &QGraphicsScene::focusItemChanged, this,
            [this](QGraphicsItem *newFocus, QGraphicsItem *oldFocus,
                   Qt::FocusReason) {
        if (_editingText && oldFocus == _editingText
            && newFocus != _editingText)
            finishTextEditing();
    });
    fitPage();
}

void EBBoardView::setDrawingTool(DrawingTool tool)
{
    finishKeyboardMove();
    finishAreaSelection();
    finishTextEditing();
    if (!_movingObjects.isEmpty())
        finishObjectMove();
    // 切换只影响下一次操作，进行中的笔迹继续使用按下时的工具。
    _drawingTool = tool;
    _scene->setObjectInteractionEnabled(tool == DrawingTool::Select
                                        || tool == DrawingTool::Text);
    // 平移只改变视图，不建立笔迹或撤销历史。
    setDragMode(tool == DrawingTool::Pan ? QGraphicsView::ScrollHandDrag
                                         : QGraphicsView::NoDrag);
    emit drawingToolChanged(tool);
}

EBBoardView::DrawingTool EBBoardView::drawingTool() const
{
    return _drawingTool;
}

bool EBBoardView::canUndo() const
{
    return !_editActive && _undoStack->canUndo();
}

bool EBBoardView::canRedo() const
{
    return !_editActive && _undoStack->canRedo();
}

void EBBoardView::undo()
{
    if (!canUndo())
        return;
    _undoStack->undo();
    syncCurrentPageStrokes();
    emit historyAvailabilityChanged(canUndo(), canRedo());
}

void EBBoardView::redo()
{
    if (!canRedo())
        return;
    _undoStack->redo();
    syncCurrentPageStrokes();
    emit historyAvailabilityChanged(canUndo(), canRedo());
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

int EBBoardView::addPage()
{
    finishPageInteraction();
    const int index = _document->addPage();
    emit pageListChanged();
    setCurrentPageIndex(index);
    return index;
}

int EBBoardView::duplicateCurrentPage()
{
    finishPageInteraction();
    const int index = _document->duplicatePage(_document->currentPageIndex());
    if (index < 0)
        return -1;
    emit pageListChanged();
    setCurrentPageIndex(index);
    return index;
}

bool EBBoardView::removeCurrentPage()
{
    if (_document->pageCount() <= 1)
        return false;
    finishPageInteraction();
    if (!_document->removePage(_document->currentPageIndex()))
        return false;
    showCurrentPage();
    emit pageListChanged();
    emit currentPageChanged(_document->currentPageIndex());
    return true;
}

bool EBBoardView::moveCurrentPage(int offset)
{
    const int from = _document->currentPageIndex();
    const int to = from + offset;
    if (to < 0 || to >= _document->pageCount())
        return false;
    finishPageInteraction();
    if (!_document->movePage(from, to))
        return false;
    emit pageListChanged();
    emit currentPageChanged(_document->currentPageIndex());
    return true;
}

bool EBBoardView::setCurrentPageIndex(int index)
{
    if (!_document->pageAt(index))
        return false;
    if (index == _document->currentPageIndex())
        return true;

    // 先结束旧页上的拖动，再载入新页，撤销命令不能跨页恢复笔迹。
    finishPageInteraction();
    _document->setCurrentPageIndex(index);
    showCurrentPage();
    emit currentPageChanged(index);
    return true;
}

void EBBoardView::commitCurrentPage()
{
    // 保存前结束尚未释放鼠标的操作，确保模型包含场景中的最新笔迹。
    finishPageInteraction();
}

void EBBoardView::reloadDocument()
{
    // 文档对象已由主窗口整体替换，丢弃旧文档的交互与撤销状态。
    _activeStroke = nullptr;
    _movingObjects.clear();
    _moveStartPositions.clear();
    _editingText = nullptr;
    _editingTextWasNew = false;
    _erasing = false;
    _pointing = false;
    _editActive = false;
    _editChanged = false;
    _editBefore = Snapshot();
    _scene->hidePointer();
    showCurrentPage();
    emit pageListChanged();
    emit currentPageChanged(_document->currentPageIndex());
}

void EBBoardView::zoomIn()
{
    zoomBy(kZoomStep);
}

void EBBoardView::zoomOut()
{
    zoomBy(1.0 / kZoomStep);
}

qreal EBBoardView::zoomFactor() const
{
    return _zoomFactor;
}

void EBBoardView::setPageColor(PageColor color)
{
    if (pageColor() == color)
        return;
    _document->currentPage()->setColor(color);
    _scene->setPageColor(color);
    emit pageContentChanged(_document->currentPageIndex());
}

EBBoardView::PageColor EBBoardView::pageColor() const
{
    return _scene->pageColor();
}

void EBBoardView::setPagePattern(PagePattern pattern)
{
    if (pagePattern() == pattern)
        return;
    _document->currentPage()->setPattern(pattern);
    _scene->setPagePattern(pattern);
    emit pageContentChanged(_document->currentPageIndex());
}

EBBoardView::PagePattern EBBoardView::pagePattern() const
{
    return _scene->pagePattern();
}

void EBBoardView::setPageSize(PageSize size)
{
    if (pageSize() == size)
        return;
    finishPageInteraction();
    _document->currentPage()->setSize(size);
    _scene->setPageSize(size);
    fitPage();
    emit pageContentChanged(_document->currentPageIndex());
}

EBBoardView::PageSize EBBoardView::pageSize() const
{
    return _scene->pageSize();
}

QRectF EBBoardView::pageRect() const
{
    return _scene->pageRect();
}

QPointF EBBoardView::toPagePosition(const QPointF &viewportPosition) const
{
    // 视口坐标先映射到场景，再减去页面在场景中的左上角。
    return mapToScene(viewportPosition.toPoint()) - pageRect().topLeft();
}

void EBBoardView::resizeEvent(QResizeEvent *event)
{
    // 直接使用上次保存的场景中心；此时视口尺寸可能已变化。
    QGraphicsView::resizeEvent(event);
    applyViewState();
}

void EBBoardView::showEvent(QShowEvent *event)
{
    QGraphicsView::showEvent(event);
    // 从其他工作区返回时，按保存的缩放与中心恢复视图。
    applyViewState();
}

void EBBoardView::wheelEvent(QWheelEvent *event)
{
    if (!(event->modifiers() & Qt::ControlModifier)) {
        QGraphicsView::wheelEvent(event);
        _viewCenter = mapToScene(viewport()->rect().center());
        return;
    }

    const QPointF before = mapToScene(event->pos());
    zoomBy(event->angleDelta().y() >= 0 ? kZoomStep : 1.0 / kZoomStep);
    // Ctrl + 滚轮时保持鼠标所指的场景位置尽量不动。
    _viewCenter = mapToScene(viewport()->rect().center())
                  + before - mapToScene(event->pos());
    centerOn(_viewCenter);
    event->accept();
}

void EBBoardView::keyPressEvent(QKeyEvent *event)
{
    if (_editingText
        && (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
        && (event->modifiers() & Qt::ControlModifier)) {
        finishTextEditing();
        setFocus();
        event->accept();
        return;
    }
    const bool arrow = event->key() == Qt::Key_Left
        || event->key() == Qt::Key_Right
        || event->key() == Qt::Key_Up
        || event->key() == Qt::Key_Down;
    const bool blockedModifier = event->modifiers()
        & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier);
    if (!_editingText && arrow && !blockedModifier
        && _scene->selectedObjectsEditable()) {
        const qreal distance = event->modifiers() & Qt::ShiftModifier
            ? 10.0 : 1.0;
        QPointF delta;
        if (event->key() == Qt::Key_Left)
            delta.setX(-distance);
        else if (event->key() == Qt::Key_Right)
            delta.setX(distance);
        else if (event->key() == Qt::Key_Up)
            delta.setY(-distance);
        else
            delta.setY(distance);
        nudgeSelectedObjects(delta);
        event->accept();
        return;
    }
    QGraphicsView::keyPressEvent(event);
}

void EBBoardView::keyReleaseEvent(QKeyEvent *event)
{
    const bool arrow = event->key() == Qt::Key_Left
        || event->key() == Qt::Key_Right
        || event->key() == Qt::Key_Up
        || event->key() == Qt::Key_Down;
    if (_keyboardMoving && arrow && !event->isAutoRepeat()) {
        finishKeyboardMove();
        event->accept();
        return;
    }
    QGraphicsView::keyReleaseEvent(event);
}

void EBBoardView::mousePressEvent(QMouseEvent *event)
{
    finishKeyboardMove();
    if (_drawingTool == DrawingTool::Pan) {
        QGraphicsView::mousePressEvent(event);
        return;
    }
    const QPointF scenePosition = mapToScene(event->pos());
    const QPointF pagePosition = scenePosition - pageRect().topLeft();
    if (_drawingTool == DrawingTool::Text
        && event->button() == Qt::LeftButton
        && pageRect().contains(scenePosition)) {
        EBTextItem *text = _scene->textAt(scenePosition);
        if (text && _scene->isObjectLocked(text)) {
            finishTextEditing();
            _scene->clearSelection();
            _scene->setObjectSelected(text, true);
            event->accept();
            return;
        }
        if (_editingText == text && text) {
            QGraphicsView::mousePressEvent(event);
            return;
        }
        finishTextEditing();
        beginEdit();
        const bool newlyCreated = !text;
        if (!text) {
            QFont font;
            font.setPointSize(22);
            text = _scene->addText(QString(), font,
                                   ebThemeColor(EBThemeColor::BoardPen));
            text->setPos(scenePosition);
        }
        startTextEditing(text, newlyCreated);
        event->accept();
        return;
    }
    if (_drawingTool == DrawingTool::Text) {
        QGraphicsView::mousePressEvent(event);
        return;
    }
    if (_drawingTool == DrawingTool::Select
        && event->button() == Qt::LeftButton) {
        QGraphicsItem *object = pageRect().contains(scenePosition)
            ? _scene->objectAt(scenePosition) : nullptr;
        const bool extendSelection = event->modifiers()
            & (Qt::ControlModifier | Qt::ShiftModifier);
        if (!object) {
            if (pageRect().contains(scenePosition))
                startAreaSelection(event->pos(), event->modifiers());
            else if (!extendSelection)
                _scene->clearSelection();
            event->accept();
            return;
        }
        if (extendSelection) {
            _scene->setObjectSelected(object, !object->isSelected());
            event->accept();
            return;
        }
        if (!object->isSelected()) {
            _scene->clearSelection();
            _scene->setObjectSelected(object, true);
        }
        if (!_scene->selectedObjectsEditable()) {
            event->accept();
            return;
        }
        _activeTool = DrawingTool::Select;
        _movingObjects = _scene->selectedObjects();
        _moveStartPositions.clear();
        for (QGraphicsItem *selected : _movingObjects)
            _moveStartPositions.append(selected->pos());
        _moveStartScene = scenePosition;
        beginEdit();
        event->accept();
        return;
    }
    if (event->button() != Qt::LeftButton
        || !QRectF(QPointF(), pageRect().size()).contains(pagePosition)) {
        QGraphicsView::mousePressEvent(event);
        return;
    }

    _activeTool = _drawingTool;
    if (_activeTool == DrawingTool::Eraser) {
        beginEdit();
        _erasing = true;
        _lastEraserPosition = pagePosition;
        _editChanged |= _scene->eraseAt(pagePosition, kEraserRadius);
        event->accept();
        return;
    }
    if (_activeTool == DrawingTool::Pointer) {
        _pointing = true;
        _scene->showPointerAt(pagePosition);
        event->accept();
        return;
    }

    beginEdit();
    _strokeStart = pagePosition;
    QPainterPath path(pagePosition);
    // 极短首段使单次点击也形成可见圆头笔点。
    path.lineTo(pagePosition + QPointF(0.01, 0.0));
    const bool marker = _activeTool == DrawingTool::Marker;
    const QPen pen(marker ? ebThemeColor(EBThemeColor::BoardMarker)
                          : ebThemeColor(EBThemeColor::BoardPen),
                   marker ? kMarkerWidth : kPenWidth, Qt::SolidLine,
                   Qt::RoundCap, Qt::RoundJoin);
    _activeStroke = _scene->addStroke(path, pen);
    _editChanged = true;
    event->accept();
}

void EBBoardView::mouseMoveEvent(QMouseEvent *event)
{
    const QPointF scenePosition = mapToScene(event->pos());
    emit pagePositionChanged(scenePosition - pageRect().topLeft(),
                             pageRect().contains(scenePosition));

    if (_selectingArea && (event->buttons() & Qt::LeftButton)) {
        updateAreaSelection(event->pos());
        event->accept();
        return;
    }

    if (!_movingObjects.isEmpty() && (event->buttons() & Qt::LeftButton)) {
        updateObjectMove(scenePosition);
        event->accept();
        return;
    }
    if (_erasing && (event->buttons() & Qt::LeftButton)) {
        const QPointF current = boundedPagePosition(event->pos());
        eraseAlong(_lastEraserPosition, current);
        _lastEraserPosition = current;
        event->accept();
        return;
    }
    if (_pointing && (event->buttons() & Qt::LeftButton)) {
        if (_scene->pointerVisible())
            _scene->showPointerAt(scenePosition - pageRect().topLeft());
        event->accept();
        return;
    }
    if (_activeStroke && (event->buttons() & Qt::LeftButton)) {
        updateActiveStroke(boundedPagePosition(event->pos()));
        event->accept();
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
    if (_drawingTool == DrawingTool::Pan && (event->buttons() & Qt::LeftButton))
        _viewCenter = mapToScene(viewport()->rect().center());
}

void EBBoardView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && _selectingArea) {
        updateAreaSelection(event->pos());
        finishAreaSelection();
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton && !_movingObjects.isEmpty()) {
        updateObjectMove(mapToScene(event->pos()));
        finishObjectMove();
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton && (_erasing || _pointing)) {
        if (_erasing)
            eraseAlong(_lastEraserPosition, boundedPagePosition(event->pos()));
        _erasing = false;
        _pointing = false;
        _scene->hidePointer();
        finishEdit();
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton && _activeStroke) {
        updateActiveStroke(boundedPagePosition(event->pos()));
        _activeStroke = nullptr;
        finishEdit();
        event->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
    if (_drawingTool == DrawingTool::Pan && event->button() == Qt::LeftButton)
        _viewCenter = mapToScene(viewport()->rect().center());
}

void EBBoardView::leaveEvent(QEvent *event)
{
    _scene->hidePointer();
    emit pagePositionChanged(QPointF(), false);
    QGraphicsView::leaveEvent(event);
}

void EBBoardView::hideEvent(QHideEvent *event)
{
    _viewCenter = mapToScene(viewport()->rect().center());
    // 模式切换可能发生在鼠标释放前，结束本次编辑以免保留失效图元指针。
    finishTextEditing();
    finishAreaSelection();
    finishKeyboardMove();
    finishObjectMove();
    _activeStroke = nullptr;
    _erasing = false;
    _pointing = false;
    _scene->hidePointer();
    finishEdit();
    QGraphicsView::hideEvent(event);
}

void EBBoardView::fitPage()
{
    _zoomFactor = 1.0;
    _viewCenter = _scene->sceneRect().center();
    _viewStateReady = true;
    applyViewState();
}

void EBBoardView::applyViewState()
{
    if (!_viewStateReady || viewport()->width() <= 0 || viewport()->height() <= 0)
        return;
    resetTransform();
    fitInView(_scene->sceneRect(), Qt::KeepAspectRatio);
    scale(_zoomFactor, _zoomFactor);
    centerOn(_viewCenter);
}

void EBBoardView::zoomBy(qreal factor)
{
    const qreal next = qBound(kMinZoomFactor, _zoomFactor * factor, kMaxZoomFactor);
    if (qFuzzyCompare(next, _zoomFactor))
        return;
    _zoomFactor = next;
    applyViewState();
}

QPointF EBBoardView::boundedPagePosition(const QPointF &viewportPosition) const
{
    const QPointF position = toPagePosition(viewportPosition);
    return QPointF(qBound(0.0, position.x(), pageRect().width()),
                   qBound(0.0, position.y(), pageRect().height()));
}

void EBBoardView::updateActiveStroke(const QPointF &pagePosition)
{
    if (_activeTool == DrawingTool::Line) {
        // 直线每次从固定起点重建路径，不保留中途拖动点。
        QPainterPath line(_strokeStart);
        line.lineTo(pagePosition);
        _activeStroke->setPath(line);
    } else {
        QPainterPath path = _activeStroke->path();
        path.lineTo(pagePosition);
        _activeStroke->setPath(path);
    }
    // 新笔迹尚未发生变换时，持续以当前几何中心作为后续缩放和旋转中心。
    _activeStroke->setTransformOriginPoint(
        _activeStroke->path().boundingRect().center());
}

void EBBoardView::eraseAlong(const QPointF &from, const QPointF &to)
{
    // 相邻事件之间补点，快速拖动也能连续擦到中间笔迹。
    const int steps = qMax(1, qCeil(QLineF(from, to).length() / kEraserRadius));
    for (int index = 1; index <= steps; ++index) {
        const QPointF position = from + (to - from) * (qreal(index) / steps);
        _editChanged |= _scene->eraseAt(position, kEraserRadius);
    }
}

void EBBoardView::updateObjectMove(const QPointF &scenePosition)
{
    if (_movingObjects.isEmpty())
        return;
    const QPointF delta = scenePosition - _moveStartScene;
    for (int index = 0; index < _movingObjects.size(); ++index)
        _movingObjects.at(index)->setPos(_moveStartPositions.at(index) + delta);
    keepObjectsInsidePage(_movingObjects);
}

void EBBoardView::finishObjectMove()
{
    if (_movingObjects.isEmpty())
        return;
    for (int index = 0; index < _movingObjects.size(); ++index)
        _editChanged |= _movingObjects.at(index)->pos()
                        != _moveStartPositions.at(index);
    _movingObjects.clear();
    _moveStartPositions.clear();
    finishEdit();
}

void EBBoardView::startAreaSelection(
    const QPoint &viewportPosition, Qt::KeyboardModifiers modifiers)
{
    _selectingArea = true;
    _selectionOrigin = viewportPosition;
    _selectionModifiers = modifiers;
    _selectionBaseline = _scene->selectedObjects();
    _selectionBand->setGeometry(QRect(_selectionOrigin, QSize()));
    _selectionBand->show();
    updateAreaSelection(viewportPosition);
}

void EBBoardView::updateAreaSelection(const QPoint &viewportPosition)
{
    if (!_selectingArea)
        return;
    const QRect viewportRect = QRect(_selectionOrigin, viewportPosition)
        .normalized().intersected(viewport()->rect());
    _selectionBand->setGeometry(viewportRect);
    const QRectF sceneRect = mapToScene(viewportRect).boundingRect()
        .intersected(pageRect());
    const QVector<QGraphicsItem *> hits = sceneRect.isEmpty()
        ? QVector<QGraphicsItem *>() : _scene->objectsInRect(sceneRect);

    QSet<QGraphicsItem *> desired;
    const bool toggle = _selectionModifiers & Qt::ControlModifier;
    const bool extend = _selectionModifiers & Qt::ShiftModifier;
    if (toggle || extend) {
        for (QGraphicsItem *object : _selectionBaseline)
            desired.insert(object);
    }
    for (QGraphicsItem *object : hits) {
        if (toggle && desired.contains(object))
            desired.remove(object);
        else
            desired.insert(object);
    }
    _scene->clearSelection();
    for (QGraphicsItem *object : desired)
        object->setSelected(true);
}

void EBBoardView::finishAreaSelection()
{
    if (!_selectingArea)
        return;
    _selectionBand->hide();
    _selectionBaseline.clear();
    _selectionModifiers = Qt::NoModifier;
    _selectingArea = false;
}

void EBBoardView::nudgeSelectedObjects(const QPointF &delta)
{
    const QVector<QGraphicsItem *> objects = _scene->selectedObjects();
    if (!_scene->selectedObjectsEditable()
        || (qFuzzyIsNull(delta.x()) && qFuzzyIsNull(delta.y())))
        return;
    if (!_keyboardMoving) {
        _activeTool = DrawingTool::Select;
        beginEdit();
        _keyboardMoving = true;
    }
    QVector<QPointF> before;
    before.reserve(objects.size());
    for (QGraphicsItem *object : objects) {
        before.append(object->pos());
        object->moveBy(delta.x(), delta.y());
    }
    keepObjectsInsidePage(objects);
    for (int index = 0; index < objects.size(); ++index)
        _editChanged |= objects.at(index)->pos() != before.at(index);
}

void EBBoardView::finishKeyboardMove()
{
    if (!_keyboardMoving)
        return;
    _keyboardMoving = false;
    finishEdit();
}

void EBBoardView::startTextEditing(EBTextItem *text, bool newlyCreated)
{
    if (!text)
        return;
    _activeTool = DrawingTool::Text;
    _editingText = text;
    _editingTextWasNew = newlyCreated;
    _textBeforeEdit = text->toPlainText();
    _scene->clearSelection();
    text->setSelected(true);
    text->setTextInteractionFlags(Qt::TextEditorInteraction);
    text->setFocus(Qt::MouseFocusReason);
    QTextCursor cursor = text->textCursor();
    cursor.movePosition(QTextCursor::End);
    text->setTextCursor(cursor);
}

void EBBoardView::finishTextEditing()
{
    if (!_editingText)
        return;
    EBTextItem *text = _editingText;
    const bool newlyCreated = _editingTextWasNew;
    const QString before = _textBeforeEdit;
    const QString after = text->toPlainText();
    _editingText = nullptr;
    _editingTextWasNew = false;
    _textBeforeEdit.clear();
    text->setTextInteractionFlags(Qt::NoTextInteraction);
    text->clearFocus();
    if (after.isEmpty()) {
        delete text;
        _editChanged = !newlyCreated;
    } else {
        text->refreshTransformOrigin();
        keepObjectInsidePage(text);
        _editChanged = newlyCreated || after != before;
    }
    finishEdit();
    emit selectionAvailabilityChanged(hasSelectedObject());
}

void EBBoardView::keepObjectInsidePage(QGraphicsItem *object)
{
    if (!object)
        return;
    const QRectF page = pageRect();
    QRectF bounds = object->sceneBoundingRect();
    if (bounds.width() > page.width() || bounds.height() > page.height()) {
        const qreal fit = qMin(page.width() / bounds.width(),
                               page.height() / bounds.height());
        object->setScale(object->scale() * fit);
        bounds = object->sceneBoundingRect();
    }
    QPointF offset;
    if (bounds.width() <= page.width()) {
        if (bounds.left() < page.left())
            offset.rx() = page.left() - bounds.left();
        else if (bounds.right() > page.right())
            offset.rx() = page.right() - bounds.right();
    }
    if (bounds.height() <= page.height()) {
        if (bounds.top() < page.top())
            offset.ry() = page.top() - bounds.top();
        else if (bounds.bottom() > page.bottom())
            offset.ry() = page.bottom() - bounds.bottom();
    }
    object->moveBy(offset.x(), offset.y());
}

void EBBoardView::keepObjectsInsidePage(
    const QVector<QGraphicsItem *> &objects)
{
    if (objects.isEmpty())
        return;
    QRectF bounds;
    const QRectF page = pageRect();
    for (QGraphicsItem *object : objects) {
        const QRectF objectBounds = object->sceneBoundingRect();
        if (objectBounds.width() > page.width()
            || objectBounds.height() > page.height())
            keepObjectInsidePage(object);
        bounds = bounds.isNull() ? object->sceneBoundingRect()
                                 : bounds.united(object->sceneBoundingRect());
    }
    QPointF offset;
    if (bounds.width() <= page.width()) {
        if (bounds.left() < page.left())
            offset.rx() = page.left() - bounds.left();
        else if (bounds.right() > page.right())
            offset.rx() = page.right() - bounds.right();
    }
    if (bounds.height() <= page.height()) {
        if (bounds.top() < page.top())
            offset.ry() = page.top() - bounds.top();
        else if (bounds.bottom() > page.bottom())
            offset.ry() = page.bottom() - bounds.bottom();
    }
    for (QGraphicsItem *object : objects)
        object->moveBy(offset.x(), offset.y());
}

void EBBoardView::commitObjectEdit(const Snapshot &before,
                                   const QString &description)
{
    _undoStack->push(new PageEditCommand(this, before,
                                           _scene->captureSnapshot(), description));
    syncCurrentPageStrokes();
    emit historyAvailabilityChanged(canUndo(), canRedo());
}

void EBBoardView::moveSelectedObjectLayer(EBBoardScene::LayerMove move,
                                          const QString &description)
{
    finishTextEditing();
    finishObjectMove();
    const Snapshot before = _scene->captureSnapshot();
    if (!_scene->moveSelectedObject(move))
        return;
    commitObjectEdit(before, description);
}

void EBBoardView::restoreSnapshot(const Snapshot &snapshot)
{
    _movingObjects.clear();
    _moveStartPositions.clear();
    _editingText = nullptr;
    _scene->restoreSnapshot(snapshot);
}

void EBBoardView::beginEdit()
{
    _editBefore = _scene->captureSnapshot();
    _editActive = true;
    _editChanged = false;
    emit historyAvailabilityChanged(false, false);
}

void EBBoardView::finishEdit()
{
    if (!_editActive)
        return;
    _editActive = false;
    if (_editChanged) {
        QString description = tr("绘制笔迹");
        if (_activeTool == DrawingTool::Eraser)
            description = tr("局部擦除");
        else if (_activeTool == DrawingTool::Select)
            description = tr("移动对象");
        else if (_activeTool == DrawingTool::Text)
            description = tr("编辑文本");
        _undoStack->push(new PageEditCommand(this, _editBefore,
                                               _scene->captureSnapshot(), description));
        syncCurrentPageStrokes();
    }
    _editBefore = Snapshot();
    emit historyAvailabilityChanged(canUndo(), canRedo());
}

void EBBoardView::finishPageInteraction()
{
    finishTextEditing();
    finishObjectMove();
    finishAreaSelection();
    finishKeyboardMove();
    _activeStroke = nullptr;
    _erasing = false;
    _pointing = false;
    _scene->hidePointer();
    finishEdit();
}

void EBBoardView::showCurrentPage()
{
    finishAreaSelection();
    finishKeyboardMove();
    _movingObjects.clear();
    _moveStartPositions.clear();
    _editingText = nullptr;
    _undoStack->clear();
    _scene->showPage(*_document->currentPage());
    fitPage();
    emit historyAvailabilityChanged(false, false);
    emit pagePositionChanged(QPointF(), false);
}

void EBBoardView::syncCurrentPageStrokes()
{
    // 场景中的临时指示点不进入页面模型；只同步可恢复的页面对象。
    _document->currentPage()->setStrokes(_scene->captureStrokes());
    _document->currentPage()->setTexts(_scene->captureTexts());
    _document->currentPage()->setImages(_scene->captureImages());
    emit pageContentChanged(_document->currentPageIndex());
}
