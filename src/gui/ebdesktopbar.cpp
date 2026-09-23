#include "ebdesktopbar.h"

#include <QAction>
#include <QActionGroup>

#include "ebbarstyle.h"
#include "ebicons.h"

EBDesktopBar::EBDesktopBar(QWidget *parent)
    : QToolBar(tr("桌面批注"), parent)
    , _undoAction(nullptr)
    , _redoAction(nullptr)
{
    setObjectName(QStringLiteral("desktopAnnotationBar"));
    ebStyleBar(this, QStringLiteral("border: 1px solid #DCE6EA; border-radius: 10px;"));
    QActionGroup *tools = new QActionGroup(this);
    tools->setExclusive(true);
    const char *icons[] = {"pen", "marker", "eraser"};
    const char *names[] = {"desktopPenAction", "desktopMarkerAction",
                           "desktopEraserAction"};
    const QString labels[] = {tr("画笔"), tr("荧光笔"), tr("橡皮")};
    for (int index = 0; index < 3; ++index) {
        QAction *action = addAction(ebToolbarIcon(icons[index]), labels[index]);
        action->setObjectName(QString::fromLatin1(names[index]));
        action->setCheckable(true);
        tools->addAction(action);
        connect(action, &QAction::triggered, this, [this, index]() {
            emit toolSelected(static_cast<EBDesktopOverlay::Tool>(index));
        });
        if (index == 0)
            action->setChecked(true);
    }
    addSeparator();
    _undoAction = addAction(ebToolbarIcon("undo"), tr("撤销"));
    _undoAction->setObjectName(QStringLiteral("desktopUndoAction"));
    _undoAction->setShortcut(QKeySequence::Undo);
    _redoAction = addAction(ebToolbarIcon("redo"), tr("重做"));
    _redoAction->setObjectName(QStringLiteral("desktopRedoAction"));
    _redoAction->setShortcut(QKeySequence::Redo);
    connect(_undoAction, &QAction::triggered,
            this, &EBDesktopBar::undoRequested);
    connect(_redoAction, &QAction::triggered,
            this, &EBDesktopBar::redoRequested);
    addSeparator();
    QAction *exit = addAction(ebToolbarIcon("file_exit"), tr("返回白板"));
    exit->setObjectName(QStringLiteral("desktopExitAction"));
    connect(exit, &QAction::triggered,
            this, &EBDesktopBar::exitRequested);
    setHistory(false, false);
}

void EBDesktopBar::setHistory(bool undoAvailable, bool redoAvailable)
{
    _undoAction->setEnabled(undoAvailable);
    _redoAction->setEnabled(redoAvailable);
}
