#include "ebboardview.h"

#include <QUndoCommand>
#include <QUndoStack>

#include "../domain/ebdocument.h"

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
    _snapReferences.clear();
    hideSnapGuides();
    _editingText = nullptr;
    _scene->restoreSnapshot(snapshot);
    refreshManualGuides();
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
        QString description = _editDescription.isEmpty()
            ? tr("绘制笔迹") : _editDescription;
        if (_editDescription.isEmpty() && _activeTool == DrawingTool::Eraser)
            description = tr("局部擦除");
        else if (_editDescription.isEmpty() && _activeTool == DrawingTool::Select)
            description = tr("移动对象");
        else if (_editDescription.isEmpty() && _activeTool == DrawingTool::Text)
            description = tr("编辑文本");
        _undoStack->push(new PageEditCommand(this, _editBefore,
                                               _scene->captureSnapshot(), description));
        syncCurrentPageStrokes();
    }
    _editBefore = Snapshot();
    _editDescription.clear();
    emit historyAvailabilityChanged(canUndo(), canRedo());
}

void EBBoardView::finishPageInteraction()
{
    if (_teachingTools.isInteracting()) {
        _teachingTools.cancel();
        syncTeachingTools();
    }
    finishGuideDrag(false);
    finishObjectTransform();
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
    _teachingTools.cancel();
    _teachingTools.clear();
    emit teachingToolChanged(EBTeachingTools::Kind::None);
    finishAreaSelection();
    finishKeyboardMove();
    _movingObjects.clear();
    _moveStartPositions.clear();
    _snapReferences.clear();
    hideSnapGuides();
    _editingText = nullptr;
    _undoStack->clear();
    _scene->showPage(*_document->currentPage());
    _teachingTools.restore(_document->currentPage()->teachingTools(), pageRect());
    refreshManualGuides();
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
    syncTeachingTools();
    _document->currentPage()->setHorizontalGuides(_scene->horizontalGuides());
    _document->currentPage()->setVerticalGuides(_scene->verticalGuides());
    emit pageContentChanged(_document->currentPageIndex());
}


