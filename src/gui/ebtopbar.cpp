#include "ebtopbar.h"

#include <QAction>
#include <QActionGroup>
#include <QMainWindow>
#include <QMenu>
#include <QSizePolicy>
#include <QSignalBlocker>
#include <QToolButton>

#include "../board/ebboardview.h"
#include "ebbarstyle.h"
#include "ebicons.h"
#include "ebpagesizedialog.h"

EBTopBar::EBTopBar(QMainWindow *window, EBBoardView *boardView,
                   QAction *insertImageAction)
    : QToolBar(tr("工作模式"), window)
    , _boardView(boardView)
    , _insertImageAction(insertImageAction)
{
    setObjectName(QStringLiteral("modeToolBar"));
    window->addToolBar(Qt::TopToolBarArea, this);
    ebStyleBar(this, QStringLiteral("border-bottom: 1px solid #DCE6EA;"), true);
    createModeActions();
    addSeparator();
    _pagePanelAction = addAction(ebToolbarIcon("page_sidebar"),
                                  tr("显示页面栏"));
    _pagePanelAction->setObjectName(QStringLiteral("pageNavigatorAction"));
    _pagePanelAction->setToolTip(tr("显示或隐藏左侧页面栏"));
    _pagePanelAction->setCheckable(true);
    _pagePanelAction->setChecked(true);
    connect(_pagePanelAction, &QAction::toggled,
            this, &EBTopBar::pagePanelVisibilityRequested);
    _displayAction = addAction(ebToolbarIcon("display_view"), tr("展示视图"));
    _displayAction->setObjectName(QStringLiteral("displayViewAction"));
    _displayAction->setToolTip(tr("打开或关闭独立展示视图"));
    _displayAction->setCheckable(true);
    connect(_displayAction, &QAction::toggled,
            this, &EBTopBar::displayViewRequested);
    QWidget *spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    addWidget(spacer);
    createBackgroundMenu();
    _insertImageAction->setIcon(ebToolbarIcon("image_object"));
    _insertImageAction->setToolTip(tr("插入图片对象"));
    addAction(_insertImageAction);
    addSeparator();
    createZoomActions();
    addSeparator();
    _undoAction = addAction(ebToolbarIcon("undo"), tr("撤销"));
    _undoAction->setObjectName(QStringLiteral("undoAction"));
    _undoAction->setShortcut(QKeySequence::Undo);
    _undoAction->setEnabled(false);
    _redoAction = addAction(ebToolbarIcon("redo"), tr("重做"));
    _redoAction->setObjectName(QStringLiteral("redoAction"));
    _redoAction->setShortcut(QKeySequence::Redo);
    _redoAction->setEnabled(false);
    connect(_undoAction, &QAction::triggered, _boardView, &EBBoardView::undo);
    connect(_redoAction, &QAction::triggered, _boardView, &EBBoardView::redo);
    connect(_boardView, &EBBoardView::currentPageChanged,
            this, &EBTopBar::syncPage);
    connect(_boardView, &EBBoardView::historyAvailabilityChanged,
            this, [this](bool canUndo, bool canRedo) {
        _undoAction->setEnabled(_boardModeActive && canUndo);
        _redoAction->setEnabled(_boardModeActive && canRedo);
    });
    syncPage();
}

void EBTopBar::retranslate()
{
    setWindowTitle(tr("工作模式"));
    const struct { const char *name; const char *source; } labels[] = {
        {"boardModeAction", "白板"},
        {"documentModeAction", "文档"},
        {"webModeAction", "网页"},
        {"desktopModeAction", "桌面"},
        {"pageNavigatorAction", "显示页面栏"},
        {"displayViewAction", "展示视图"},
        {"zoomInAction", "放大"},
        {"zoomOutAction", "缩小"},
        {"fitPageAction", "适应页面"},
        {"undoAction", "撤销"},
        {"redoAction", "重做"},
        {"whitePageAction", "纯白"},
        {"creamPageAction", "暖白"},
        {"blankPatternAction", "空白"},
        {"gridPatternAction", "网格"},
        {"ruledPatternAction", "横线"},
        {"standardPageAction", "常规 4:3"},
        {"widescreenPageAction", "宽屏 16:9"},
        {"customPageAction", "自定义尺寸..."},
    };
    for (const auto &entry : labels) {
        if (QAction *action = findChild<QAction *>(QString::fromLatin1(entry.name)))
            action->setText(tr(entry.source));
    }
    const QString modes[] = {tr("白板"), tr("文档"), tr("网页"), tr("桌面")};
    for (int index = 0; index < 4; ++index)
        _modeActions[index]->setToolTip(modes[index]);
    _pagePanelAction->setToolTip(tr("显示或隐藏左侧页面栏"));
    _displayAction->setToolTip(tr("打开或关闭独立展示视图"));
    _insertImageAction->setToolTip(tr("插入图片对象"));
    _backgroundButton->setText(tr("背景"));
    _backgroundButton->setToolTip(tr("背景"));
    _backgroundButton->setAccessibleName(tr("背景"));
    if (QMenu *menu = _backgroundButton->menu()) {
        const QString labels[] = {tr("页面底色"), tr("页面底纹"), tr("页面尺寸")};
        for (int index = 0; index < 3 && index < menu->actions().size(); ++index)
            menu->actions().at(index)->setText(labels[index]);
    }
    _zoomInAction->setToolTip(tr("放大"));
    _zoomOutAction->setToolTip(tr("缩小"));
    _fitPageAction->setToolTip(tr("适应页面"));
}

void EBTopBar::setMode(EBApplicationController::MainMode mode)
{
    const int index = static_cast<int>(mode);
    if (index < 0 || index >= 4)
        return;
    _modeActions[index]->setChecked(true);
    _boardModeActive = mode == EBApplicationController::MainMode::Board;
    _pagePanelAction->setEnabled(_boardModeActive);
    _displayAction->setEnabled(_boardModeActive);
    _backgroundButton->setEnabled(_boardModeActive);
    _insertImageAction->setEnabled(_boardModeActive);
    _zoomInAction->setEnabled(_boardModeActive);
    _zoomOutAction->setEnabled(_boardModeActive);
    _fitPageAction->setEnabled(_boardModeActive);
    _undoAction->setEnabled(_boardModeActive && _boardView->canUndo());
    _redoAction->setEnabled(_boardModeActive && _boardView->canRedo());
}

QString EBTopBar::modeLabel(EBApplicationController::MainMode mode) const
{
    const int index = static_cast<int>(mode);
    return index >= 0 && index < 4 ? _modeActions[index]->text() : QString();
}

void EBTopBar::setDisplayVisible(bool visible)
{
    const QSignalBlocker blocker(_displayAction);
    _displayAction->setChecked(visible);
}

void EBTopBar::syncPage()
{
    const int color = _boardView->pageColor() == EBBoardView::PageColor::White
        ? 0 : 1;
    int pattern = 0;
    if (_boardView->pagePattern() == EBBoardView::PagePattern::Grid)
        pattern = 1;
    else if (_boardView->pagePattern() == EBBoardView::PagePattern::Ruled)
        pattern = 2;
    const int size = _boardView->pageSize() == EBBoardView::PageSize::Standard
        ? 0 : _boardView->pageSize() == EBBoardView::PageSize::Widescreen ? 1 : 2;
    _colorActions[color]->setChecked(true);
    _patternActions[pattern]->setChecked(true);
    _sizeActions[size]->setChecked(true);
}

void EBTopBar::createZoomActions()
{
    _zoomInAction = this->addAction(tr("放大"));
    _zoomInAction->setObjectName(QStringLiteral("zoomInAction"));
    _zoomInAction->setIcon(ebToolbarIcon("zoom_in"));
    _zoomInAction->setToolTip(tr("放大"));
    _zoomOutAction = this->addAction(tr("缩小"));
    _zoomOutAction->setObjectName(QStringLiteral("zoomOutAction"));
    _zoomOutAction->setIcon(ebToolbarIcon("zoom_out"));
    _zoomOutAction->setToolTip(tr("缩小"));
    _fitPageAction = this->addAction(tr("适应页面"));
    _fitPageAction->setObjectName(QStringLiteral("fitPageAction"));
    _fitPageAction->setIcon(ebToolbarIcon("fit_page"));
    _fitPageAction->setToolTip(tr("适应页面"));
    _fitPageAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
    connect(_zoomInAction, &QAction::triggered, _boardView, &EBBoardView::zoomIn);
    connect(_zoomOutAction, &QAction::triggered, _boardView, &EBBoardView::zoomOut);
    connect(_fitPageAction, &QAction::triggered, _boardView, &EBBoardView::fitPage);
}

void EBTopBar::createBackgroundMenu()
{
    _backgroundButton = new QToolButton(this);
    _backgroundButton->setObjectName(QStringLiteral("backgroundToolButton"));
    _backgroundButton->setText(tr("背景"));
    _backgroundButton->setIcon(ebToolbarIcon("background"));
    _backgroundButton->setIconSize(QSize(24, 24));
    _backgroundButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    _backgroundButton->setToolTip(tr("背景"));
    _backgroundButton->setAccessibleName(tr("背景"));
    _backgroundButton->setPopupMode(QToolButton::InstantPopup);
    QMenu *backgroundMenu = new QMenu(_backgroundButton);
    QMenu *colorMenu = backgroundMenu->addMenu(ebToolbarIcon("page_color"),
                                                tr("页面底色"));
    QMenu *patternMenu = backgroundMenu->addMenu(ebToolbarIcon("background"),
                                                  tr("页面底纹"));
    QMenu *sizeMenu = backgroundMenu->addMenu(ebToolbarIcon("page_size"),
                                               tr("页面尺寸"));
    _backgroundButton->setMenu(backgroundMenu);
    this->addWidget(_backgroundButton);

    QActionGroup *colorGroup = new QActionGroup(this);
    QActionGroup *patternGroup = new QActionGroup(this);
    QActionGroup *sizeGroup = new QActionGroup(this);
    colorGroup->setExclusive(true);
    patternGroup->setExclusive(true);
    sizeGroup->setExclusive(true);
    const auto choice = [](QMenu *menu, QActionGroup *group,
                           const QString &label, const char *name,
                           const char *icon) {
        QAction *action = menu->addAction(ebToolbarIcon(icon), label);
        action->setObjectName(QString::fromLatin1(name));
        action->setCheckable(true);
        group->addAction(action);
        return action;
    };
    QAction *white = choice(colorMenu, colorGroup, tr("纯白"),
                            "whitePageAction", "page_white");
    QAction *cream = choice(colorMenu, colorGroup, tr("暖白"),
                            "creamPageAction", "page_cream");
    QAction *blank = choice(patternMenu, patternGroup, tr("空白"),
                            "blankPatternAction", "page_blank");
    QAction *grid = choice(patternMenu, patternGroup, tr("网格"),
                           "gridPatternAction", "page_grid");
    QAction *ruled = choice(patternMenu, patternGroup, tr("横线"),
                            "ruledPatternAction", "page_ruled");
    QAction *standard = choice(sizeMenu, sizeGroup, tr("常规 4:3"),
                               "standardPageAction", "page_standard");
    QAction *widescreen = choice(sizeMenu, sizeGroup, tr("宽屏 16:9"),
                                 "widescreenPageAction", "page_wide");
    QAction *custom = choice(sizeMenu, sizeGroup, tr("自定义尺寸..."),
                             "customPageAction", "page_size");
    _colorActions[0] = white;
    _colorActions[1] = cream;
    _patternActions[0] = blank;
    _patternActions[1] = grid;
    _patternActions[2] = ruled;
    _sizeActions[0] = standard;
    _sizeActions[1] = widescreen;
    _sizeActions[2] = custom;
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
    connect(custom, &QAction::triggered, _boardView, [this]() {
        EBPageSizeDialog dialog(_boardView->pageRect().size(), _boardView);
        if (dialog.exec() == QDialog::Accepted)
            _boardView->setCustomPageSize(dialog.pageSize());
        syncPage();
    });
}

void EBTopBar::createModeActions()
{
    QActionGroup *group = new QActionGroup(this);
    group->setExclusive(true);
    const QString labels[] = {tr("白板"), tr("文档"), tr("网页"), tr("桌面")};
    const char *names[] = {"boardModeAction", "documentModeAction",
                           "webModeAction", "desktopModeAction"};
    const char *icons[] = {"board", "document", "web", "desktop"};
    for (int index = 0; index < 4; ++index) {
        QAction *action = new QAction(ebToolbarIcon(icons[index]), labels[index], this);
        action->setObjectName(QString::fromLatin1(names[index]));
        action->setToolTip(labels[index]);
        action->setCheckable(true);
        group->addAction(action);
        this->addAction(action);
        _modeActions[index] = action;
        connect(action, &QAction::triggered, this, [this, index]() {
            emit modeRequested(static_cast<EBApplicationController::MainMode>(index));
        });
    }
}
