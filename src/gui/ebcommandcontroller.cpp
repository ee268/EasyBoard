#include "ebcommandcontroller.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QIcon>
#include <QMainWindow>
#include <QToolButton>

#include "../board/ebboardview.h"

QIcon EBCommandController::toolbarIcon(const char *name)
{
    return QIcon(QStringLiteral(":/toolbar/icons/toolbar/")
                 + QLatin1String(name) + QStringLiteral(".svg"));
}

EBCommandController::EBCommandController(QMainWindow *window, EBBoardView *boardView)
    : QObject(window)
    , _window(window)
    , _boardView(boardView)
{
    createFileMenu();
    createEditMenu();
    createToolBar();
    connect(_boardView, &EBBoardView::currentPageChanged, this, [this](int) {
        // 切页后菜单勾选状态跟随当前页的背景和尺寸设置。
        const int color = _boardView->pageColor() == EBBoardView::PageColor::White ? 0 : 1;
        int pattern = 0;
        if (_boardView->pagePattern() == EBBoardView::PagePattern::Grid)
            pattern = 1;
        else if (_boardView->pagePattern() == EBBoardView::PagePattern::Ruled)
            pattern = 2;
        _colorActions[color]->setChecked(true);
        _patternActions[pattern]->setChecked(true);
        const int size = _boardView->pageSize() == EBBoardView::PageSize::Standard ? 0 : 1;
        _sizeActions[size]->setChecked(true);
        updateSelectAllAction();
        updateTextFormatAction();
    });
    // 拖动中禁用历史动作；完成编辑后按实际栈状态重新启用。
    connect(_boardView, &EBBoardView::historyAvailabilityChanged,
            this, [this](bool canUndo, bool canRedo) {
        const bool onBoard = _modeActions[0] && _modeActions[0]->isChecked();
        _undoAction->setEnabled(onBoard && canUndo);
        _redoAction->setEnabled(onBoard && canRedo);
        updateLayerActions();
        updateGroupActions();
        updateLockActions();
        updateArrangeActions();
        updateSelectAllAction();
        updateTextFormatAction();
    });
    connect(_boardView, &EBBoardView::selectionAvailabilityChanged,
            this, [this](bool) {
        updateObjectActions();
        updateClipboardActions();
        updateLayerActions();
        updateGroupActions();
        updateLockActions();
        updateArrangeActions();
        updateSelectAllAction();
        updateTextFormatAction();
    });
    connect(_boardView, &EBBoardView::drawingToolChanged, this,
            [this](EBBoardView::DrawingTool tool) {
        const int index = static_cast<int>(tool);
        if (index >= 0 && index < 11 && _toolActions[index])
            _toolActions[index]->setChecked(true);
    });
    connect(QApplication::clipboard(), &QClipboard::dataChanged,
            this, &EBCommandController::updateClipboardActions);
}

void EBCommandController::setMode(EBApplicationController::MainMode mode)
{
    const int index = static_cast<int>(mode);
    if (index < 0 || index >= 4)
        return;
    _modeActions[index]->setChecked(true);
    const bool onBoard = mode == EBApplicationController::MainMode::Board;
    _boardModeActive = onBoard;
    for (QAction *tool : _toolActions)
        tool->setEnabled(onBoard);
    _backgroundButton->setEnabled(onBoard);
    _brushButton->setEnabled(onBoard);
    _shapeButton->setEnabled(onBoard);
    _zoomInAction->setEnabled(onBoard);
    _zoomOutAction->setEnabled(onBoard);
    _fitPageAction->setEnabled(onBoard);
    _insertImageObjectAction->setEnabled(onBoard);
    _snapAction->setEnabled(onBoard);
    _gridSnapAction->setEnabled(onBoard);
    _undoAction->setEnabled(onBoard && _boardView->canUndo());
    _redoAction->setEnabled(onBoard && _boardView->canRedo());
    updateObjectActions();
    updateClipboardActions();
    updateLayerActions();
    updateGroupActions();
    updateLockActions();
    updateArrangeActions();
    updateSelectAllAction();
    updateTextFormatAction();
}

void EBCommandController::updateObjectActions()
{
    const bool enabled = _boardModeActive
        && _boardView->hasEditableSelectedObjects();
    for (QAction *action : _objectActions) {
        if (action)
            action->setEnabled(enabled);
    }
}

void EBCommandController::updateClipboardActions()
{
    const bool selected = _boardModeActive && _boardView->hasSelectedObject();
    const bool editable = _boardModeActive
        && _boardView->hasEditableSelectedObjects();
    if (_cutAction)
        _cutAction->setEnabled(editable);
    if (_copyAction)
        _copyAction->setEnabled(selected);
    if (_duplicateAction)
        _duplicateAction->setEnabled(selected);
    if (_pasteAction) {
        _pasteAction->setEnabled(_boardModeActive
                                 && !_boardView->isTextEditing()
                                 && _boardView->canPasteObject());
    }
}

void EBCommandController::updateLayerActions()
{
    const bool canMoveBackward = _boardModeActive
        && _boardView->canMoveSelectedObjectBackward();
    const bool canMoveForward = _boardModeActive
        && _boardView->canMoveSelectedObjectForward();
    if (_layerActions[0])
        _layerActions[0]->setEnabled(canMoveBackward);
    if (_layerActions[1])
        _layerActions[1]->setEnabled(canMoveBackward);
    if (_layerActions[2])
        _layerActions[2]->setEnabled(canMoveForward);
    if (_layerActions[3])
        _layerActions[3]->setEnabled(canMoveForward);
}

void EBCommandController::updateGroupActions()
{
    if (_groupAction) {
        _groupAction->setEnabled(_boardModeActive
                                 && _boardView->canGroupSelectedObjects());
    }
    if (_ungroupAction) {
        _ungroupAction->setEnabled(_boardModeActive
                                   && _boardView->canUngroupSelectedObjects());
    }
}

void EBCommandController::updateLockActions()
{
    if (_lockAction) {
        _lockAction->setEnabled(_boardModeActive
                                && _boardView->canLockSelectedObjects());
    }
    if (_unlockAction) {
        _unlockAction->setEnabled(_boardModeActive
                                  && _boardView->canUnlockSelectedObjects());
    }
}

void EBCommandController::updateArrangeActions()
{
    bool available = false;
    for (int index = 0; index < 8; ++index) {
        const bool enabled = _boardModeActive
            && _boardView->canArrangeSelectedObjects(
                static_cast<EBBoardView::ObjectArrangement>(index));
        if (_arrangeActions[index])
            _arrangeActions[index]->setEnabled(enabled);
        available |= enabled;
    }
    if (_arrangeButton)
        _arrangeButton->setEnabled(available);
}

void EBCommandController::updateSelectAllAction()
{
    if (_selectAllAction) {
        _selectAllAction->setEnabled(_boardModeActive
                                     && _boardView->canSelectAllObjects());
    }
}

void EBCommandController::updateTextFormatAction()
{
    if (_textFormatButton)
        _textFormatButton->setEnabled(_boardModeActive
            && _boardView->canFormatSelectedText());
}

QString EBCommandController::modeLabel(EBApplicationController::MainMode mode) const
{
    const int index = static_cast<int>(mode);
    return index >= 0 && index < 4 ? _modeActions[index]->text() : QString();
}
