#include "ebcommands.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QMainWindow>

#include "../board/ebboardview.h"
#include "ebdrawbar.h"
#include "ebobjectbar.h"
#include "ebtopbar.h"

EBCommands::EBCommands(QMainWindow *window, EBBoardView *boardView)
    : QObject(window)
    , _window(window)
    , _boardView(boardView)
    , _topBar(nullptr)
    , _drawBar(nullptr)
    , _objectBar(nullptr)
{
    createFileMenu();
    createEditMenu();
    _topBar = new EBTopBar(window, boardView, _insertImageObjectAction);
    _drawBar = new EBDrawBar(window, boardView);
    EBObjectBar::Actions actions{};
    actions.cut = _cutAction;
    actions.copy = _copyAction;
    actions.paste = _pasteAction;
    actions.duplicate = _duplicateAction;
    actions.group = _groupAction;
    actions.ungroup = _ungroupAction;
    actions.lock = _lockAction;
    actions.unlock = _unlockAction;
    for (int index = 0; index < 4; ++index)
        actions.layers[index] = _layerActions[index];
    for (int index = 0; index < 8; ++index)
        actions.arrangements[index] = _arrangeActions[index];
    _objectBar = new EBObjectBar(window, boardView, actions);
    connect(_topBar, &EBTopBar::modeRequested,
            this, &EBCommands::modeRequested);
    connect(_topBar, &EBTopBar::pagePanelVisibilityRequested,
            this, &EBCommands::pagePanelVisibilityRequested);
    connect(_topBar, &EBTopBar::displayViewRequested,
            this, &EBCommands::displayViewRequested);
    connect(_boardView, &EBBoardView::currentPageChanged,
            this, &EBCommands::refreshActions);
    connect(_boardView, &EBBoardView::historyAvailabilityChanged,
            this, &EBCommands::refreshActions);
    connect(_boardView, &EBBoardView::selectionAvailabilityChanged,
            this, &EBCommands::refreshActions);
    connect(QApplication::clipboard(), &QClipboard::dataChanged,
            this, &EBCommands::updateClipboardActions);
}

void EBCommands::setMode(EBApplicationController::MainMode mode)
{
    const int index = static_cast<int>(mode);
    if (index < 0 || index >= 4)
        return;
    _boardModeActive = mode == EBApplicationController::MainMode::Board;
    _topBar->setMode(mode);
    _drawBar->setBoardActive(_boardModeActive);
    _objectBar->setBoardActive(_boardModeActive);
    _snapAction->setEnabled(_boardModeActive);
    _gridSnapAction->setEnabled(_boardModeActive);
    refreshActions();
}

QString EBCommands::modeLabel(EBApplicationController::MainMode mode) const
{
    return _topBar->modeLabel(mode);
}

void EBCommands::setDisplayVisible(bool visible)
{
    _topBar->setDisplayVisible(visible);
}

void EBCommands::refreshActions()
{
    updateClipboardActions();
    updateLayerActions();
    updateGroupActions();
    updateLockActions();
    updateArrangeActions();
    updateSelectAllAction();
}

void EBCommands::updateClipboardActions()
{
    const bool selected = _boardModeActive && _boardView->hasSelectedObject();
    const bool editable = _boardModeActive
        && _boardView->hasEditableSelectedObjects();
    _cutAction->setEnabled(editable);
    _copyAction->setEnabled(selected);
    _duplicateAction->setEnabled(selected);
    _pasteAction->setEnabled(_boardModeActive
                             && !_boardView->isTextEditing()
                             && _boardView->canPasteObject());
}

void EBCommands::updateLayerActions()
{
    const bool backward = _boardModeActive
        && _boardView->canMoveSelectedObjectBackward();
    const bool forward = _boardModeActive
        && _boardView->canMoveSelectedObjectForward();
    _layerActions[0]->setEnabled(backward);
    _layerActions[1]->setEnabled(backward);
    _layerActions[2]->setEnabled(forward);
    _layerActions[3]->setEnabled(forward);
}

void EBCommands::updateGroupActions()
{
    _groupAction->setEnabled(_boardModeActive
        && _boardView->canGroupSelectedObjects());
    _ungroupAction->setEnabled(_boardModeActive
        && _boardView->canUngroupSelectedObjects());
}

void EBCommands::updateLockActions()
{
    _lockAction->setEnabled(_boardModeActive
        && _boardView->canLockSelectedObjects());
    _unlockAction->setEnabled(_boardModeActive
        && _boardView->canUnlockSelectedObjects());
}

void EBCommands::updateArrangeActions()
{
    bool available = false;
    for (int index = 0; index < 8; ++index) {
        const bool enabled = _boardModeActive
            && _boardView->canArrangeSelectedObjects(
                static_cast<EBBoardView::ObjectArrangement>(index));
        _arrangeActions[index]->setEnabled(enabled);
        available |= enabled;
    }
    _objectBar->setArrangeAvailable(available);
}

void EBCommands::updateSelectAllAction()
{
    _selectAllAction->setEnabled(_boardModeActive
        && _boardView->canSelectAllObjects());
}
