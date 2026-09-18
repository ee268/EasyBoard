#include "ebmainwindow.h"

#include "../core/ebsettings.h"

#include "../board/ebboardview.h"

#include <QDebug>
#include <QMenuBar>
#include <QLabel>
#include <QFileDialog>
#include <QToolBar>
#include <QStackedWidget>
#include <QStatusBar>

EBMainWindow::EBMainWindow(QWidget *parent)
    : QMainWindow{parent}
    , _modeStack(new QStackedWidget(this))
{
    setWindowTitle(tr("EasyBoard"));

    const QByteArray geometry = EBSettings::settings()->windowGeometry();
    if (geometry.isEmpty() || !restoreGeometry(geometry))
        resize(960, 640);

    qDebug() << "主窗口尺寸: " << size();

    QMenu* fileMenu = menuBar()->addMenu(tr("文件(&F)"));
    QAction* openAction = fileMenu->addAction(tr("打开..."));
    openAction->setObjectName(QStringLiteral("openFileAction"));
    openAction->setShortcut(QKeySequence::Open);

    connect(openAction, &QAction::triggered, this, [this](){
        const QString path = QFileDialog::getOpenFileName(this, tr("选择要打开的文件"));
        if (!path.isEmpty()) {
            emit fileImportRequested(path);
        }
    });
    fileMenu->addSeparator();

    QAction* quitAction = fileMenu->addAction(tr("退出"));
    quitAction->setObjectName(QStringLiteral("quitAction"));
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, this, &EBMainWindow::quitRequested);

    // 同一 QAction 同时用于菜单和工具栏，勾选状态不会出现两套不同步的副本。
    QToolBar *modeToolBar = addToolBar(tr("工作模式"));
    modeToolBar->setObjectName(QStringLiteral("modeToolBar"));
    modeToolBar->setToolButtonStyle(Qt::ToolButtonTextOnly);

    QActionGroup *drawingGroup = new QActionGroup(this);
    drawingGroup->setExclusive(true);
    const auto addDrawingTool = [this, modeToolBar, drawingGroup](
                                    const QString &name, const QString &actionName) {
        QAction *action = new QAction(name, this);
        action->setObjectName(actionName);
        action->setCheckable(true);
        drawingGroup->addAction(action);
        modeToolBar->addAction(action);
        return action;
    };
    QAction *penAction = addDrawingTool(tr("画笔"), QStringLiteral("penToolAction"));
    QAction *markerAction = addDrawingTool(tr("荧光笔"), QStringLiteral("markerToolAction"));
    QAction *lineAction = addDrawingTool(tr("直线"), QStringLiteral("lineToolAction"));
    QAction *eraserAction = addDrawingTool(tr("橡皮"), QStringLiteral("eraserToolAction"));
    QAction *pointerAction = addDrawingTool(tr("指示"), QStringLiteral("pointerToolAction"));
    penAction->setChecked(true);
    modeToolBar->addSeparator();

    QActionGroup *modeGroup = new QActionGroup(this);
    modeGroup->setExclusive(true);

    QWidget* toolbarSpacer = new QWidget(modeToolBar);
    toolbarSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    modeToolBar->addWidget(toolbarSpacer);

    // 四个占位页使阶段 7 的切换能被看到；真实功能会逐页替换它们。
    const auto addMode = [=](
                             EBApplicationController::MainMode mode,
                             const QString &name,
                             const QString &actionName) {
        QAction *action = new QAction(name, this);
        action->setObjectName(actionName);
        action->setCheckable(true);
        modeGroup->addAction(action);
        modeToolBar->addAction(action);
        connect(action, &QAction::triggered, this, [this, mode]() {
            emit modeRequested(mode);
        });

        if (mode == EBApplicationController::MainMode::Board){
            EBBoardView* boardView = new EBBoardView(_modeStack);
            connect(boardView, &EBBoardView::pagePositionChanged,
                    this, [this](const QPointF &pagePosition, bool insidePage){
                if (insidePage) {
                    statusBar()->showMessage(
                        tr("页面坐标：(%1, %2)")
                            .arg(qRound(pagePosition.x()))
                            .arg(qRound(pagePosition.y())));
                } else {
                    statusBar()->clearMessage();
                }
            });

            connect(penAction, &QAction::triggered, boardView, [boardView]() {
                boardView->setDrawingTool(EBBoardView::DrawingTool::Pen);
            });
            connect(markerAction, &QAction::triggered, boardView, [boardView]() {
                boardView->setDrawingTool(EBBoardView::DrawingTool::Marker);
            });
            connect(lineAction, &QAction::triggered, boardView, [boardView]() {
                boardView->setDrawingTool(EBBoardView::DrawingTool::Line);
            });
            connect(eraserAction, &QAction::triggered, boardView, [boardView]() {
                boardView->setDrawingTool(EBBoardView::DrawingTool::Eraser);
            });
            connect(pointerAction, &QAction::triggered, boardView, [boardView]() {
                boardView->setDrawingTool(EBBoardView::DrawingTool::Pointer);
            });

            _modeStack->addWidget(boardView);
        }
        else {
            QLabel *placeholder = new QLabel(
                tr("%1工作区\n\n阶段 7：仅演示模式切换，实际功能尚未接入").arg(name),
                _modeStack);
            placeholder->setAlignment(Qt::AlignCenter);
            _modeStack->addWidget(placeholder);
        }
    };

    addMode(EBApplicationController::MainMode::Board, tr("白板"),
            QStringLiteral("boardModeAction"));
    addMode(EBApplicationController::MainMode::Document, tr("文档"),
            QStringLiteral("documentModeAction"));
    addMode(EBApplicationController::MainMode::Web, tr("网页"),
            QStringLiteral("webModeAction"));
    addMode(EBApplicationController::MainMode::Desktop, tr("桌面"),
            QStringLiteral("desktopModeAction"));

    _modeStack->setObjectName(QStringLiteral("modeStack"));
    setCentralWidget(_modeStack);
}

void EBMainWindow::showMode(EBApplicationController::MainMode mode)
{
    const int index = static_cast<int>(mode);
    if (index < 0 || index >= _modeStack->count())
        return;

    // 栈索引与模式枚举一一对应，当前动作也随控制器状态同步勾选。
    _modeStack->setCurrentIndex(index);
    const char *actionNames[] = {
        "boardModeAction", "documentModeAction",
        "webModeAction", "desktopModeAction"
    };

    // 非白板页没有绘图表面，禁用工具但保留选中项供返回白板后继续使用。
    for (const char *name : {"penToolAction", "markerToolAction", "lineToolAction",
                             "eraserToolAction", "pointerToolAction"}) {
        QAction *tool = findChild<QAction *>(QString::fromLatin1(name));
        if (tool)
            tool->setEnabled(mode == EBApplicationController::MainMode::Board);
    }

    QAction *action = findChild<QAction *>(QString::fromLatin1(actionNames[index]));
    if (action)
        action->setChecked(true);

}
