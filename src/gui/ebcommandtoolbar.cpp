#include "ebcommandcontroller.h"

#include <QAction>
#include <QActionGroup>
#include <QIcon>
#include <QMainWindow>
#include <QMenu>
#include <QSizePolicy>
#include <QToolBar>
#include <QToolButton>

#include "../board/ebboardview.h"

void EBCommandController::createToolBar()
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
    _insertImageObjectAction->setIcon(toolbarIcon("image_object"));
    _insertImageObjectAction->setToolTip(tr("插入图片对象"));
    toolBar->addAction(_insertImageObjectAction);
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
    _cutAction->setIcon(toolbarIcon("cut"));
    _copyAction->setIcon(toolbarIcon("copy"));
    _pasteAction->setIcon(toolbarIcon("paste"));
    _duplicateAction->setIcon(toolbarIcon("duplicate"));
    _cutAction->setToolTip(tr("剪切"));
    _copyAction->setToolTip(tr("复制"));
    _pasteAction->setToolTip(tr("粘贴"));
    _duplicateAction->setToolTip(tr("快速复制（Ctrl+D）"));
    toolBar->addAction(_cutAction);
    toolBar->addAction(_copyAction);
    toolBar->addAction(_pasteAction);
    toolBar->addAction(_duplicateAction);
    _groupAction->setIcon(toolbarIcon("object_group"));
    _ungroupAction->setIcon(toolbarIcon("object_ungroup"));
    _groupAction->setToolTip(tr("组合（Ctrl+G）"));
    _ungroupAction->setToolTip(tr("取消组合（Ctrl+Shift+G）"));
    toolBar->addAction(_groupAction);
    toolBar->addAction(_ungroupAction);
    _lockAction->setToolTip(tr("锁定对象（Ctrl+L）"));
    _unlockAction->setToolTip(tr("解锁对象（Ctrl+Shift+L）"));
    toolBar->addAction(_lockAction);
    toolBar->addAction(_unlockAction);
    _arrangeButton = new QToolButton(toolBar);
    _arrangeButton->setObjectName(QStringLiteral("arrangeObjectsButton"));
    _arrangeButton->setIcon(toolbarIcon("object_arrange"));
    _arrangeButton->setToolTip(tr("对齐与分布"));
    _arrangeButton->setPopupMode(QToolButton::InstantPopup);
    _arrangeButton->setEnabled(false);
    QMenu *arrangeMenu = new QMenu(_arrangeButton);
    for (int index = 0; index < 8; ++index) {
        if (index == 3 || index == 6)
            arrangeMenu->addSeparator();
        arrangeMenu->addAction(_arrangeActions[index]);
    }
    _arrangeButton->setMenu(arrangeMenu);
    toolBar->addWidget(_arrangeButton);
    toolBar->addSeparator();
    const char *layerIcons[] = {"object_to_back", "object_backward",
                                "object_forward", "object_to_front"};
    for (int index = 0; index < 4; ++index) {
        _layerActions[index]->setIcon(toolbarIcon(layerIcons[index]));
        _layerActions[index]->setToolTip(_layerActions[index]->text());
        toolBar->addAction(_layerActions[index]);
    }
    toolBar->addSeparator();
    createObjectActions(toolBar);
    toolBar->addSeparator();
    QWidget *spacer = new QWidget(toolBar);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolBar->addWidget(spacer);
    createModeActions(toolBar);
}

void EBCommandController::createZoomActions(QToolBar *toolBar)
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

void EBCommandController::createBackgroundMenu(QToolBar *toolBar)
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

void EBCommandController::createDrawingActions(QToolBar *toolBar)
{
    QActionGroup *group = new QActionGroup(this);
    group->setExclusive(true);
    const QString labels[] = {tr("选择"), tr("文本"), tr("画笔"), tr("荧光笔"),
                              tr("直线"), tr("橡皮"), tr("指示"), tr("平移")};
    const char *names[] = {"selectToolAction", "textToolAction", "penToolAction",
                           "markerToolAction", "lineToolAction", "eraserToolAction",
                           "pointerToolAction", "panToolAction"};
    const char *icons[] = {"select", "text", "pen", "marker", "line", "eraser",
                           "pointer", "pan"};
    const EBBoardView::DrawingTool tools[] = {
        EBBoardView::DrawingTool::Select, EBBoardView::DrawingTool::Text,
        EBBoardView::DrawingTool::Pen,
        EBBoardView::DrawingTool::Marker,
        EBBoardView::DrawingTool::Line, EBBoardView::DrawingTool::Eraser,
        EBBoardView::DrawingTool::Pointer, EBBoardView::DrawingTool::Pan
    };
    for (int index = 0; index < 8; ++index) {
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
    _toolActions[1]->setToolTip(tr("文本（Ctrl+Enter 完成编辑）"));
    _toolActions[2]->setChecked(true);
}

void EBCommandController::createObjectActions(QToolBar *toolBar)
{
    const QString labels[] = {tr("放大对象"), tr("缩小对象"),
                              tr("逆时针旋转"), tr("顺时针旋转"),
                              tr("删除对象")};
    const char *names[] = {"scaleObjectUpAction", "scaleObjectDownAction",
                           "rotateObjectLeftAction", "rotateObjectRightAction",
                           "deleteObjectAction"};
    const char *icons[] = {"object_scale_up", "object_scale_down",
                           "object_rotate_left", "object_rotate_right",
                           "object_delete"};
    for (int index = 0; index < 5; ++index) {
        QAction *action = toolBar->addAction(toolbarIcon(icons[index]),
                                              labels[index]);
        action->setObjectName(QString::fromLatin1(names[index]));
        action->setToolTip(labels[index]);
        action->setEnabled(false);
        _objectActions[index] = action;
    }
    connect(_objectActions[0], &QAction::triggered, _boardView,
            [this]() { _boardView->scaleSelectedObject(1.1); });
    connect(_objectActions[1], &QAction::triggered, _boardView,
            [this]() { _boardView->scaleSelectedObject(1.0 / 1.1); });
    connect(_objectActions[2], &QAction::triggered, _boardView,
            [this]() { _boardView->rotateSelectedObject(-15.0); });
    connect(_objectActions[3], &QAction::triggered, _boardView,
            [this]() { _boardView->rotateSelectedObject(15.0); });
    _objectActions[4]->setShortcut(QKeySequence::Delete);
    connect(_objectActions[4], &QAction::triggered,
            _boardView, &EBBoardView::deleteSelectedObject);
}

void EBCommandController::createModeActions(QToolBar *toolBar)
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
