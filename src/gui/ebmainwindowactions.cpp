#include "ebmainwindowactions.h"

#include <QAction>
#include <QActionGroup>
#include <QFileDialog>
#include <QIcon>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QSizePolicy>
#include <QToolBar>
#include <QToolButton>

#include "../board/ebboardview.h"

namespace {
QIcon toolbarIcon(const char *name)
{
    return QIcon(QStringLiteral(":/toolbar/icons/toolbar/")
                 + QLatin1String(name) + QStringLiteral(".svg"));
}
}

EBMainWindowActions::EBMainWindowActions(QMainWindow *window, EBBoardView *boardView)
    : QObject(window)
    , _window(window)
    , _boardView(boardView)
{
    createFileMenu();
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
    });
    // 拖动中禁用历史动作；完成编辑后按实际栈状态重新启用。
    connect(_boardView, &EBBoardView::historyAvailabilityChanged,
            this, [this](bool canUndo, bool canRedo) {
        const bool onBoard = _modeActions[0] && _modeActions[0]->isChecked();
        _undoAction->setEnabled(onBoard && canUndo);
        _redoAction->setEnabled(onBoard && canRedo);
    });
}

void EBMainWindowActions::createFileMenu()
{
    QMenu *fileMenu = _window->menuBar()->addMenu(tr("文件(&F)"));
    QAction *importAction = fileMenu->addAction(tr("打开..."));
    importAction->setObjectName(QStringLiteral("openFileAction"));
    importAction->setShortcut(QKeySequence::Open);
    connect(importAction, &QAction::triggered, this, [this]() {
        // 菜单只收集现有文件路径，业务处理仍由应用层完成。
        const QString path = QFileDialog::getOpenFileName(
            _window, tr("打开 EasyBoard 文档"), QString(),
            tr("EasyBoard 文档 (*.json)"));
        if (!path.isEmpty())
            emit fileImportRequested(path);
    });

    QAction *saveAction = fileMenu->addAction(tr("保存"));
    saveAction->setObjectName(QStringLiteral("saveDocumentAction"));
    saveAction->setShortcut(QKeySequence::Save);
    connect(saveAction, &QAction::triggered,
            this, &EBMainWindowActions::saveDocumentRequested);

    fileMenu->addSeparator();
    QAction *quitAction = fileMenu->addAction(tr("退出"));
    quitAction->setObjectName(QStringLiteral("quitAction"));
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, this, &EBMainWindowActions::quitRequested);
}

void EBMainWindowActions::createToolBar()
{
    QToolBar *toolBar = _window->addToolBar(tr("工作模式"));
    toolBar->setObjectName(QStringLiteral("modeToolBar"));
    // 图标仅负责呈现；动作文字继续供悬停提示和辅助技术使用。
    toolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    toolBar->setIconSize(QSize(24, 24));
    toolBar->setMovable(false);
    toolBar->setFloatable(false);
    toolBar->setStyleSheet(QStringLiteral(
        "QToolBar { background: #F8FAFB; border: none; border-bottom: 1px solid #DCE6EA; padding: 5px 8px; spacing: 2px; }"
        "QToolBar QToolButton { background: transparent; border: 1px solid transparent; border-radius: 8px; min-width: 34px; min-height: 34px; padding: 3px; }"
        "QToolBar QToolButton:hover { background: #EAF4F2; }"
        "QToolBar QToolButton:checked { background: #DDF3EF; border-color: #A5DDD4; }"
        "QToolBar QToolButton:pressed { background: #C7E9E2; }"
        "QToolBar::separator { background: #D8E2E6; width: 1px; margin: 7px 6px; }"));
    createBackgroundMenu(toolBar);
    toolBar->addSeparator();

    createZoomActions(toolBar);
    toolBar->addSeparator();

    _undoAction = toolBar->addAction(tr("撤销"));
    _undoAction->setObjectName(QStringLiteral("undoAction"));
    _undoAction->setIcon(toolbarIcon("undo"));
    _undoAction->setToolTip(tr("撤销"));
    _undoAction->setShortcut(QKeySequence::Undo);
    _undoAction->setEnabled(false);
    _redoAction = toolBar->addAction(tr("重做"));
    _redoAction->setObjectName(QStringLiteral("redoAction"));
    _redoAction->setIcon(toolbarIcon("redo"));
    _redoAction->setToolTip(tr("重做"));
    _redoAction->setShortcut(QKeySequence::Redo);
    _redoAction->setEnabled(false);
    connect(_undoAction, &QAction::triggered, _boardView, &EBBoardView::undo);
    connect(_redoAction, &QAction::triggered, _boardView, &EBBoardView::redo);
    toolBar->addSeparator();

    createDrawingActions(toolBar);
    toolBar->addSeparator();
    QWidget *spacer = new QWidget(toolBar);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolBar->addWidget(spacer);
    createModeActions(toolBar);
}

void EBMainWindowActions::createZoomActions(QToolBar *toolBar)
{
    _zoomInAction = toolBar->addAction(tr("放大"));
    _zoomInAction->setObjectName(QStringLiteral("zoomInAction"));
    _zoomInAction->setIcon(toolbarIcon("zoom_in"));
    _zoomInAction->setToolTip(tr("放大"));
    _zoomOutAction = toolBar->addAction(tr("缩小"));
    _zoomOutAction->setObjectName(QStringLiteral("zoomOutAction"));
    _zoomOutAction->setIcon(toolbarIcon("zoom_out"));
    _zoomOutAction->setToolTip(tr("缩小"));
    _fitPageAction = toolBar->addAction(tr("适应页面"));
    _fitPageAction->setObjectName(QStringLiteral("fitPageAction"));
    _fitPageAction->setIcon(toolbarIcon("fit_page"));
    _fitPageAction->setToolTip(tr("适应页面"));
    _fitPageAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
    connect(_zoomInAction, &QAction::triggered, _boardView, &EBBoardView::zoomIn);
    connect(_zoomOutAction, &QAction::triggered, _boardView, &EBBoardView::zoomOut);
    connect(_fitPageAction, &QAction::triggered, _boardView, &EBBoardView::fitPage);
}

void EBMainWindowActions::createBackgroundMenu(QToolBar *toolBar)
{
    _backgroundButton = new QToolButton(toolBar);
    _backgroundButton->setObjectName(QStringLiteral("backgroundToolButton"));
    _backgroundButton->setText(tr("背景"));
    _backgroundButton->setIcon(toolbarIcon("background"));
    _backgroundButton->setIconSize(QSize(24, 24));
    _backgroundButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    _backgroundButton->setToolTip(tr("背景"));
    _backgroundButton->setAccessibleName(tr("背景"));
    _backgroundButton->setPopupMode(QToolButton::InstantPopup);
    QMenu *backgroundMenu = new QMenu(_backgroundButton);
    QMenu *colorMenu = backgroundMenu->addMenu(tr("页面底色"));
    QMenu *patternMenu = backgroundMenu->addMenu(tr("页面底纹"));
    QMenu *sizeMenu = backgroundMenu->addMenu(tr("页面尺寸"));
    _backgroundButton->setMenu(backgroundMenu);
    toolBar->addWidget(_backgroundButton);

    QActionGroup *colorGroup = new QActionGroup(this);
    QActionGroup *patternGroup = new QActionGroup(this);
    QActionGroup *sizeGroup = new QActionGroup(this);
    colorGroup->setExclusive(true);
    patternGroup->setExclusive(true);
    sizeGroup->setExclusive(true);
    const auto choice = [](QMenu *menu, QActionGroup *group,
                           const QString &label, const char *name) {
        QAction *action = menu->addAction(label);
        action->setObjectName(QString::fromLatin1(name));
        action->setCheckable(true);
        group->addAction(action);
        return action;
    };
    QAction *white = choice(colorMenu, colorGroup, tr("纯白"), "whitePageAction");
    QAction *cream = choice(colorMenu, colorGroup, tr("暖白"), "creamPageAction");
    QAction *blank = choice(patternMenu, patternGroup, tr("空白"), "blankPatternAction");
    QAction *grid = choice(patternMenu, patternGroup, tr("网格"), "gridPatternAction");
    QAction *ruled = choice(patternMenu, patternGroup, tr("横线"), "ruledPatternAction");
    QAction *standard = choice(sizeMenu, sizeGroup, tr("常规 4:3"), "standardPageAction");
    QAction *widescreen = choice(sizeMenu, sizeGroup, tr("宽屏 16:9"), "widescreenPageAction");
    _colorActions[0] = white;
    _colorActions[1] = cream;
    _patternActions[0] = blank;
    _patternActions[1] = grid;
    _patternActions[2] = ruled;
    _sizeActions[0] = standard;
    _sizeActions[1] = widescreen;
    white->setChecked(true);
    blank->setChecked(true);
    standard->setChecked(true);

    // 两组动作分别改变场景状态，互不覆盖已有的底色或底纹选择。
    connect(white, &QAction::triggered, _boardView, [this]() {
        _boardView->setPageColor(EBBoardView::PageColor::White);
    });
    connect(cream, &QAction::triggered, _boardView, [this]() {
        _boardView->setPageColor(EBBoardView::PageColor::Cream);
    });
    connect(blank, &QAction::triggered, _boardView, [this]() {
        _boardView->setPagePattern(EBBoardView::PagePattern::Blank);
    });
    connect(grid, &QAction::triggered, _boardView, [this]() {
        _boardView->setPagePattern(EBBoardView::PagePattern::Grid);
    });
    connect(ruled, &QAction::triggered, _boardView, [this]() {
        _boardView->setPagePattern(EBBoardView::PagePattern::Ruled);
    });
    connect(standard, &QAction::triggered, _boardView, [this]() {
        _boardView->setPageSize(EBBoardView::PageSize::Standard);
    });
    connect(widescreen, &QAction::triggered, _boardView, [this]() {
        _boardView->setPageSize(EBBoardView::PageSize::Widescreen);
    });
}

void EBMainWindowActions::createDrawingActions(QToolBar *toolBar)
{
    QActionGroup *group = new QActionGroup(this);
    group->setExclusive(true);
    const QString labels[] = {tr("画笔"), tr("荧光笔"), tr("直线"),
                              tr("橡皮"), tr("指示"), tr("平移")};
    const char *names[] = {"penToolAction", "markerToolAction", "lineToolAction",
                           "eraserToolAction", "pointerToolAction", "panToolAction"};
    const char *icons[] = {"pen", "marker", "line", "eraser", "pointer", "pan"};
    const EBBoardView::DrawingTool tools[] = {
        EBBoardView::DrawingTool::Pen, EBBoardView::DrawingTool::Marker,
        EBBoardView::DrawingTool::Line, EBBoardView::DrawingTool::Eraser,
        EBBoardView::DrawingTool::Pointer, EBBoardView::DrawingTool::Pan
    };
    for (int index = 0; index < 6; ++index) {
        QAction *action = new QAction(toolbarIcon(icons[index]), labels[index], this);
        action->setObjectName(QString::fromLatin1(names[index]));
        action->setToolTip(labels[index]);
        action->setCheckable(true);
        group->addAction(action);
        toolBar->addAction(action);
        _toolActions[index] = action;
        connect(action, &QAction::triggered, _boardView, [this, tool = tools[index]]() {
            _boardView->setDrawingTool(tool);
        });
    }
    _toolActions[0]->setChecked(true);
}

void EBMainWindowActions::createModeActions(QToolBar *toolBar)
{
    QActionGroup *group = new QActionGroup(this);
    group->setExclusive(true);
    const QString labels[] = {tr("白板"), tr("文档"), tr("网页"), tr("桌面")};
    const char *names[] = {"boardModeAction", "documentModeAction",
                           "webModeAction", "desktopModeAction"};
    const char *icons[] = {"board", "document", "web", "desktop"};
    for (int index = 0; index < 4; ++index) {
        QAction *action = new QAction(toolbarIcon(icons[index]), labels[index], this);
        action->setObjectName(QString::fromLatin1(names[index]));
        action->setToolTip(labels[index]);
        action->setCheckable(true);
        group->addAction(action);
        toolBar->addAction(action);
        _modeActions[index] = action;
        connect(action, &QAction::triggered, this, [this, index]() {
            emit modeRequested(static_cast<EBApplicationController::MainMode>(index));
        });
    }
}

void EBMainWindowActions::setMode(EBApplicationController::MainMode mode)
{
    const int index = static_cast<int>(mode);
    if (index < 0 || index >= 4)
        return;
    _modeActions[index]->setChecked(true);
    const bool onBoard = mode == EBApplicationController::MainMode::Board;
    for (QAction *tool : _toolActions)
        tool->setEnabled(onBoard);
    _backgroundButton->setEnabled(onBoard);
    _zoomInAction->setEnabled(onBoard);
    _zoomOutAction->setEnabled(onBoard);
    _fitPageAction->setEnabled(onBoard);
    _undoAction->setEnabled(onBoard && _boardView->canUndo());
    _redoAction->setEnabled(onBoard && _boardView->canRedo());
}

QString EBMainWindowActions::modeLabel(EBApplicationController::MainMode mode) const
{
    const int index = static_cast<int>(mode);
    return index >= 0 && index < 4 ? _modeActions[index]->text() : QString();
}
