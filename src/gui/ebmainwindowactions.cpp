#include "ebmainwindowactions.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QIcon>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QSizePolicy>
#include <QToolBar>
#include <QToolButton>

#include "../board/ebboardview.h"
#include "../import/ebimageimporter.h"
#include "../persistence/ebdocumentpackage.h"

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
    });
    // 拖动中禁用历史动作；完成编辑后按实际栈状态重新启用。
    connect(_boardView, &EBBoardView::historyAvailabilityChanged,
            this, [this](bool canUndo, bool canRedo) {
        const bool onBoard = _modeActions[0] && _modeActions[0]->isChecked();
        _undoAction->setEnabled(onBoard && canUndo);
        _redoAction->setEnabled(onBoard && canRedo);
        updateLayerActions();
        updateGroupActions();
        updateArrangeActions();
        updateSelectAllAction();
    });
    connect(_boardView, &EBBoardView::selectionAvailabilityChanged,
            this, [this](bool) {
        updateObjectActions();
        updateClipboardActions();
        updateLayerActions();
        updateGroupActions();
        updateArrangeActions();
        updateSelectAllAction();
    });
    connect(_boardView, &EBBoardView::drawingToolChanged, this,
            [this](EBBoardView::DrawingTool tool) {
        const int index = static_cast<int>(tool);
        if (index >= 0 && index < 8 && _toolActions[index])
            _toolActions[index]->setChecked(true);
    });
    connect(QApplication::clipboard(), &QClipboard::dataChanged,
            this, &EBMainWindowActions::updateClipboardActions);
}

void EBMainWindowActions::createFileMenu()
{
    QMenu *fileMenu = _window->menuBar()->addMenu(tr("文件(&F)"));
    QAction *openAction = fileMenu->addAction(tr("打开文档..."));
    openAction->setObjectName(QStringLiteral("openFileAction"));
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(
            _window, tr("打开 EasyBoard 文档"), QString(),
            tr("EasyBoard 文档 (*.json)"));
        if (!path.isEmpty())
            emit fileImportRequested(path);
    });

    QAction *importAction = fileMenu->addAction(tr("导入图片为新文档..."));
    importAction->setObjectName(QStringLiteral("importImageAction"));
    importAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_I));
    connect(importAction, &QAction::triggered, this, [this]() {
        // 菜单只收集图片路径，应用层统一处理菜单和实例间请求。
        const QString path = QFileDialog::getOpenFileName(
            _window, tr("导入图片为新文档"), QString(),
            EBImageImporter::fileDialogFilter());
        if (!path.isEmpty())
            emit fileImportRequested(path);
    });

    _insertImageObjectAction = fileMenu->addAction(tr("插入图片对象..."));
    _insertImageObjectAction->setObjectName(
        QStringLiteral("insertImageObjectAction"));
    _insertImageObjectAction->setShortcut(
        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_I));
    connect(_insertImageObjectAction, &QAction::triggered, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(
            _window, tr("插入图片对象"), QString(),
            EBImageImporter::fileDialogFilter());
        if (!path.isEmpty())
            emit imageObjectInsertRequested(path);
    });

    QAction *importPackageAction = fileMenu->addAction(tr("导入课程文档包..."));
    importPackageAction->setObjectName(
        QStringLiteral("importDocumentPackageAction"));
    connect(importPackageAction, &QAction::triggered, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(
            _window, tr("导入 EasyBoard 课程文档包"), QString(),
            EBDocumentPackage::fileDialogFilter());
        if (!path.isEmpty())
            emit fileImportRequested(path);
    });

    QAction *saveAction = fileMenu->addAction(tr("保存"));
    saveAction->setObjectName(QStringLiteral("saveDocumentAction"));
    saveAction->setShortcut(QKeySequence::Save);
    connect(saveAction, &QAction::triggered,
            this, &EBMainWindowActions::saveDocumentRequested);

    fileMenu->addSeparator();
    QAction *exportPageAction = fileMenu->addAction(tr("导出当前页图片..."));
    exportPageAction->setObjectName(QStringLiteral("exportPageImageAction"));
    connect(exportPageAction, &QAction::triggered,
            this, &EBMainWindowActions::exportPageImageRequested);
    QAction *exportPdfAction = fileMenu->addAction(tr("导出整份文档 PDF..."));
    exportPdfAction->setObjectName(QStringLiteral("exportDocumentPdfAction"));
    connect(exportPdfAction, &QAction::triggered,
            this, &EBMainWindowActions::exportDocumentPdfRequested);
    QAction *exportPackageAction = fileMenu->addAction(tr("导出课程文档包..."));
    exportPackageAction->setObjectName(
        QStringLiteral("exportDocumentPackageAction"));
    connect(exportPackageAction, &QAction::triggered,
            this, &EBMainWindowActions::exportDocumentPackageRequested);

    fileMenu->addSeparator();
    QAction *quitAction = fileMenu->addAction(tr("退出"));
    quitAction->setObjectName(QStringLiteral("quitAction"));
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, this, &EBMainWindowActions::quitRequested);
}

void EBMainWindowActions::createEditMenu()
{
    QMenu *editMenu = _window->menuBar()->addMenu(tr("编辑(&E)"));
    _cutAction = editMenu->addAction(tr("剪切"));
    _cutAction->setObjectName(QStringLiteral("cutObjectAction"));
    _cutAction->setShortcut(QKeySequence::Cut);
    _copyAction = editMenu->addAction(tr("复制"));
    _copyAction->setObjectName(QStringLiteral("copyObjectAction"));
    _copyAction->setShortcut(QKeySequence::Copy);
    _pasteAction = editMenu->addAction(tr("粘贴"));
    _pasteAction->setObjectName(QStringLiteral("pasteObjectAction"));
    _pasteAction->setShortcut(QKeySequence::Paste);
    _cutAction->setEnabled(false);
    _copyAction->setEnabled(false);
    _pasteAction->setEnabled(false);
    connect(_cutAction, &QAction::triggered,
            _boardView, &EBBoardView::cutSelectedObject);
    connect(_copyAction, &QAction::triggered,
            _boardView, &EBBoardView::copySelectedObject);
    connect(_pasteAction, &QAction::triggered,
            _boardView, &EBBoardView::pasteObject);
    _selectAllAction = editMenu->addAction(toolbarIcon("select_all"),
                                            tr("全选"));
    _selectAllAction->setObjectName(QStringLiteral("selectAllObjectsAction"));
    _selectAllAction->setShortcut(QKeySequence::SelectAll);
    _selectAllAction->setEnabled(false);
    connect(_selectAllAction, &QAction::triggered,
            _boardView, &EBBoardView::selectAllObjects);

    editMenu->addSeparator();
    _groupAction = editMenu->addAction(tr("组合"));
    _groupAction->setObjectName(QStringLiteral("groupObjectsAction"));
    _groupAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_G));
    _groupAction->setEnabled(false);
    _ungroupAction = editMenu->addAction(tr("取消组合"));
    _ungroupAction->setObjectName(QStringLiteral("ungroupObjectsAction"));
    _ungroupAction->setShortcut(
        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_G));
    _ungroupAction->setEnabled(false);
    connect(_groupAction, &QAction::triggered,
            _boardView, &EBBoardView::groupSelectedObjects);
    connect(_ungroupAction, &QAction::triggered,
            _boardView, &EBBoardView::ungroupSelectedObjects);

    QMenu *arrangeMenu = editMenu->addMenu(toolbarIcon("object_arrange"),
                                            tr("对齐与分布"));
    const QString arrangeLabels[] = {
        tr("左对齐"), tr("水平居中"), tr("右对齐"),
        tr("顶部对齐"), tr("垂直居中"), tr("底部对齐"),
        tr("水平等距分布"), tr("垂直等距分布")
    };
    const char *arrangeNames[] = {
        "alignObjectsLeftAction", "alignObjectsHorizontalCenterAction",
        "alignObjectsRightAction", "alignObjectsTopAction",
        "alignObjectsVerticalCenterAction", "alignObjectsBottomAction",
        "distributeObjectsHorizontalAction", "distributeObjectsVerticalAction"
    };
    const char *arrangeIcons[] = {
        "object_align_left", "object_align_hcenter", "object_align_right",
        "object_align_top", "object_align_vcenter", "object_align_bottom",
        "object_distribute_horizontal", "object_distribute_vertical"
    };
    for (int index = 0; index < 8; ++index) {
        if (index == 3 || index == 6)
            arrangeMenu->addSeparator();
        QAction *action = arrangeMenu->addAction(
            toolbarIcon(arrangeIcons[index]), arrangeLabels[index]);
        action->setObjectName(QString::fromLatin1(arrangeNames[index]));
        action->setEnabled(false);
        _arrangeActions[index] = action;
        connect(action, &QAction::triggered, _boardView,
                [this, index]() {
            _boardView->arrangeSelectedObjects(
                static_cast<EBBoardView::ObjectArrangement>(index));
        });
    }

    editMenu->addSeparator();
    const QString labels[] = {tr("置于底层"), tr("下移一层"),
                              tr("上移一层"), tr("置于顶层")};
    const char *names[] = {"sendObjectToBackAction", "moveObjectBackwardAction",
                           "moveObjectForwardAction", "bringObjectToFrontAction"};
    for (int index = 0; index < 4; ++index) {
        _layerActions[index] = editMenu->addAction(labels[index]);
        _layerActions[index]->setObjectName(QString::fromLatin1(names[index]));
        _layerActions[index]->setEnabled(false);
    }
    _layerActions[0]->setShortcut(
        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_BracketLeft));
    _layerActions[1]->setShortcut(
        QKeySequence(Qt::CTRL | Qt::Key_BracketLeft));
    _layerActions[2]->setShortcut(
        QKeySequence(Qt::CTRL | Qt::Key_BracketRight));
    _layerActions[3]->setShortcut(
        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_BracketRight));
    connect(_layerActions[0], &QAction::triggered,
            _boardView, &EBBoardView::sendSelectedObjectToBack);
    connect(_layerActions[1], &QAction::triggered,
            _boardView, &EBBoardView::moveSelectedObjectBackward);
    connect(_layerActions[2], &QAction::triggered,
            _boardView, &EBBoardView::moveSelectedObjectForward);
    connect(_layerActions[3], &QAction::triggered,
            _boardView, &EBBoardView::bringSelectedObjectToFront);
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
    _cutAction->setToolTip(tr("剪切"));
    _copyAction->setToolTip(tr("复制"));
    _pasteAction->setToolTip(tr("粘贴"));
    toolBar->addAction(_cutAction);
    toolBar->addAction(_copyAction);
    toolBar->addAction(_pasteAction);
    _groupAction->setIcon(toolbarIcon("object_group"));
    _ungroupAction->setIcon(toolbarIcon("object_ungroup"));
    _groupAction->setToolTip(tr("组合（Ctrl+G）"));
    _ungroupAction->setToolTip(tr("取消组合（Ctrl+Shift+G）"));
    toolBar->addAction(_groupAction);
    toolBar->addAction(_ungroupAction);
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

void EBMainWindowActions::createObjectActions(QToolBar *toolBar)
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
    _boardModeActive = onBoard;
    for (QAction *tool : _toolActions)
        tool->setEnabled(onBoard);
    _backgroundButton->setEnabled(onBoard);
    _zoomInAction->setEnabled(onBoard);
    _zoomOutAction->setEnabled(onBoard);
    _fitPageAction->setEnabled(onBoard);
    _insertImageObjectAction->setEnabled(onBoard);
    _undoAction->setEnabled(onBoard && _boardView->canUndo());
    _redoAction->setEnabled(onBoard && _boardView->canRedo());
    updateObjectActions();
    updateClipboardActions();
    updateLayerActions();
    updateGroupActions();
    updateArrangeActions();
    updateSelectAllAction();
}

void EBMainWindowActions::updateObjectActions()
{
    const bool enabled = _boardModeActive && _boardView->hasSelectedObject();
    for (QAction *action : _objectActions) {
        if (action)
            action->setEnabled(enabled);
    }
}

void EBMainWindowActions::updateClipboardActions()
{
    const bool selected = _boardModeActive && _boardView->hasSelectedObject();
    if (_cutAction)
        _cutAction->setEnabled(selected);
    if (_copyAction)
        _copyAction->setEnabled(selected);
    if (_pasteAction) {
        _pasteAction->setEnabled(_boardModeActive
                                 && !_boardView->isTextEditing()
                                 && _boardView->canPasteObject());
    }
}

void EBMainWindowActions::updateLayerActions()
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

void EBMainWindowActions::updateGroupActions()
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

void EBMainWindowActions::updateArrangeActions()
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

void EBMainWindowActions::updateSelectAllAction()
{
    if (_selectAllAction) {
        _selectAllAction->setEnabled(_boardModeActive
                                     && _boardView->canSelectAllObjects());
    }
}

QString EBMainWindowActions::modeLabel(EBApplicationController::MainMode mode) const
{
    const int index = static_cast<int>(mode);
    return index >= 0 && index < 4 ? _modeActions[index]->text() : QString();
}
