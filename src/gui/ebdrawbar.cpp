#include "ebdrawbar.h"

#include <QAction>
#include <QActionGroup>
#include <QColorDialog>
#include <QFontDialog>
#include <QInputDialog>
#include <QMainWindow>
#include <QMenu>
#include <QSizePolicy>
#include <QToolButton>

#include "../board/ebboardview.h"
#include "ebbarstyle.h"
#include "ebicons.h"

EBDrawBar::EBDrawBar(QMainWindow *window, EBBoardView *boardView)
    : QToolBar(tr("绘图工具"), window)
    , _window(window)
    , _boardView(boardView)
{
    setObjectName(QStringLiteral("drawingToolBar"));
    window->addToolBar(Qt::BottomToolBarArea, this);
    ebStyleBar(this, QStringLiteral("border-top: 1px solid #DCE6EA;"), true);
    QWidget *leftSpacer = new QWidget(this);
    leftSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    addWidget(leftSpacer);
    createDrawingActions();
    addSeparator();
    createBrushMenu();
    createTextFormatMenu();
    addSeparator();
    createTeachingMenu();
    QWidget *rightSpacer = new QWidget(this);
    rightSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    addWidget(rightSpacer);

    connect(_boardView, &EBBoardView::drawingToolChanged,
            this, [this](EBBoardView::DrawingTool tool) {
        const int index = static_cast<int>(tool);
        if (index >= 0 && index < 11 && _toolActions[index])
            _toolActions[index]->setChecked(true);
    });
    connect(_boardView, &EBBoardView::selectionAvailabilityChanged,
            this, &EBDrawBar::updateTextFormat);
    connect(_boardView, &EBBoardView::historyAvailabilityChanged,
            this, &EBDrawBar::updateTextFormat);
    connect(_boardView, &EBBoardView::currentPageChanged,
            this, &EBDrawBar::updateTextFormat);
    connect(_boardView, &EBBoardView::teachingToolChanged, this,
            [this](EBTeachingTools::Kind kind) {
        _teachingButton->setToolTip(kind == EBTeachingTools::Kind::None
                                    ? tr("教学工具") : tr("教学工具（已开启）"));
    });
}

void EBDrawBar::retranslate()
{
    setWindowTitle(tr("绘图工具"));
    const struct { const char *name; const char *source; } labels[] = {
        {"selectToolAction", "选择"},
        {"textToolAction", "文本"},
        {"penToolAction", "画笔"},
        {"markerToolAction", "荧光笔"},
        {"lineToolAction", "直线"},
        {"eraserToolAction", "橡皮"},
        {"pointerToolAction", "指示"},
        {"panToolAction", "平移"},
        {"rectangleToolAction", "矩形"},
        {"ellipseToolAction", "椭圆"},
        {"arrowToolAction", "箭头"},
        {"penColorAction", "画笔颜色..."},
        {"penWidthAction", "画笔粗细..."},
        {"markerColorAction", "荧光笔颜色..."},
        {"markerWidthAction", "荧光笔粗细..."},
        {"textFontAction", "字体和字号..."},
        {"textColorAction", "文字颜色..."},
        {"teachingRulerAction", "直尺"},
        {"teachingTriangle45Action", "45° 三角板"},
        {"teachingTriangle30Action", "30° 三角板"},
        {"teachingProtractorAction", "量角器"},
        {"teachingCompassAction", "圆规"},
        {"teachingCurtainAction", "幕布"},
        {"teachingSpotlightAction", "聚光灯"},
        {"teachingMagnifierAction", "放大镜"},
        {"teachingCircleAction", "圆规画整圆"},
        {"teachingFlipHorizontalAction", "三角板水平翻转"},
        {"teachingFlipVerticalAction", "三角板垂直翻转"},
        {"teachingResetAction", "量角器归零"},
        {"teachingMagnifierZoomInAction", "放大镜增加倍率"},
        {"teachingMagnifierZoomOutAction", "放大镜降低倍率"},
        {"teachingMagnifierShapeAction", "切换放大镜形状"},
        {"teachingHideAction", "收起教具"},
    };
    for (const auto &entry : labels) {
        if (QAction *action = findChild<QAction *>(QString::fromLatin1(entry.name)))
            action->setText(tr(entry.source));
    }
    for (QAction *action : _toolActions)
        action->setToolTip(action->text());
    _toolActions[1]->setToolTip(tr("文本（Ctrl+Enter 完成编辑）"));
    QAction *selectedShape = nullptr;
    for (int index = 8; index < 11; ++index) {
        if (_toolActions[index]->isChecked())
            selectedShape = _toolActions[index];
    }
    _shapeButton->setText(selectedShape ? selectedShape->text() : tr("形状"));
    _shapeButton->setToolTip(selectedShape ? selectedShape->text() : tr("形状工具"));
    _shapeButton->setAccessibleName(tr("形状工具"));
    for (QToolButton *button : {_brushButton, _textFormatButton, _teachingButton}) {
        const QString label = button == _brushButton ? tr("画笔")
            : button == _textFormatButton ? tr("文本") : tr("教具");
        const QString tip = button == _brushButton ? tr("画笔设置")
            : button == _textFormatButton ? tr("文字格式") : tr("教学工具");
        button->setText(label);
        button->setToolTip(tip);
        button->setAccessibleName(tip);
    }
    _teachingButton->setToolTip(_boardView->teachingTool() == EBTeachingTools::Kind::None
                                ? tr("教学工具") : tr("教学工具（已开启）"));
}

void EBDrawBar::setBoardActive(bool active)
{
    _boardModeActive = active;
    setVisible(active);
    for (QAction *tool : _toolActions)
        tool->setEnabled(active);
    _brushButton->setEnabled(active);
    _shapeButton->setEnabled(active);
    _teachingButton->setEnabled(active);
    updateTextFormat();
}

void EBDrawBar::updateTextFormat()
{
    _textFormatButton->setEnabled(_boardModeActive
        && _boardView->canFormatSelectedText());
}

void EBDrawBar::createDrawingActions()
{
    QActionGroup *group = new QActionGroup(this);
    group->setExclusive(true);
    _shapeButton = new QToolButton(this);
    _shapeButton->setObjectName(QStringLiteral("shapeToolButton"));
    _shapeButton->setIcon(ebToolbarIcon("rectangle"));
    _shapeButton->setText(tr("形状"));
    _shapeButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    _shapeButton->setToolTip(tr("形状工具"));
    _shapeButton->setAccessibleName(tr("形状工具"));
    _shapeButton->setPopupMode(QToolButton::InstantPopup);
    QMenu *shapeMenu = new QMenu(_shapeButton);
    const QString labels[] = {tr("选择"), tr("文本"), tr("画笔"), tr("荧光笔"),
                              tr("直线"), tr("橡皮"), tr("指示"), tr("平移"),
                              tr("矩形"), tr("椭圆"), tr("箭头")};
    const char *names[] = {"selectToolAction", "textToolAction", "penToolAction",
                           "markerToolAction", "lineToolAction", "eraserToolAction",
                           "pointerToolAction", "panToolAction",
                           "rectangleToolAction", "ellipseToolAction",
                           "arrowToolAction"};
    const char *icons[] = {"select", "text", "pen", "marker", "line", "eraser",
                           "pointer", "pan", "rectangle", "ellipse", "arrow"};
    const EBBoardView::DrawingTool tools[] = {
        EBBoardView::DrawingTool::Select, EBBoardView::DrawingTool::Text,
        EBBoardView::DrawingTool::Pen,
        EBBoardView::DrawingTool::Marker,
        EBBoardView::DrawingTool::Line, EBBoardView::DrawingTool::Eraser,
        EBBoardView::DrawingTool::Pointer, EBBoardView::DrawingTool::Pan,
        EBBoardView::DrawingTool::Rectangle, EBBoardView::DrawingTool::Ellipse,
        EBBoardView::DrawingTool::Arrow
    };
    for (int index = 0; index < 11; ++index) {
        QAction *action = new QAction(ebToolbarIcon(icons[index]), labels[index], this);
        action->setObjectName(QString::fromLatin1(names[index]));
        action->setToolTip(labels[index]);
        action->setCheckable(true);
        group->addAction(action);
        if (index < 8)
            this->addAction(action);
        else
            shapeMenu->addAction(action);
        _toolActions[index] = action;
        connect(action, &QAction::triggered, _boardView, [this, tool = tools[index]]() {
            _boardView->setDrawingTool(tool);
        });
        if (index >= 8) {
            connect(action, &QAction::triggered, this,
                    [this, action, icon = icons[index]]() {
                _shapeButton->setIcon(ebToolbarIcon(icon));
                _shapeButton->setText(action->text());
                _shapeButton->setToolTip(action->text());
            });
        }
    }
    _toolActions[1]->setToolTip(tr("文本（Ctrl+Enter 完成编辑）"));
    _toolActions[2]->setChecked(true);
    _shapeButton->setMenu(shapeMenu);
    this->addWidget(_shapeButton);
}

void EBDrawBar::createBrushMenu()
{
    _brushButton = new QToolButton(this);
    _brushButton->setObjectName(QStringLiteral("brushSettingsButton"));
    _brushButton->setIcon(ebToolbarIcon("brush_settings"));
    _brushButton->setText(tr("画笔"));
    _brushButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    _brushButton->setToolTip(tr("画笔设置"));
    _brushButton->setAccessibleName(tr("画笔设置"));
    _brushButton->setPopupMode(QToolButton::InstantPopup);
    QMenu *menu = new QMenu(_brushButton);
    QAction *penColor = menu->addAction(ebToolbarIcon("pen_color"),
                                        tr("画笔颜色..."));
    penColor->setObjectName(QStringLiteral("penColorAction"));
    QAction *penWidth = menu->addAction(ebToolbarIcon("pen_width"),
                                        tr("画笔粗细..."));
    penWidth->setObjectName(QStringLiteral("penWidthAction"));
    menu->addSeparator();
    QAction *markerColor = menu->addAction(ebToolbarIcon("marker_color"),
                                           tr("荧光笔颜色..."));
    markerColor->setObjectName(QStringLiteral("markerColorAction"));
    QAction *markerWidth = menu->addAction(ebToolbarIcon("marker_width"),
                                           tr("荧光笔粗细..."));
    markerWidth->setObjectName(QStringLiteral("markerWidthAction"));
    connect(penColor, &QAction::triggered, this, [this]() {
        const QColor color = QColorDialog::getColor(
            _boardView->penColor(), _window, tr("画笔颜色"));
        if (color.isValid())
            _boardView->setPenColor(color);
    });
    connect(markerColor, &QAction::triggered, this, [this]() {
        const QColor color = QColorDialog::getColor(
            _boardView->markerColor(), _window, tr("荧光笔颜色"),
            QColorDialog::ShowAlphaChannel);
        if (color.isValid())
            _boardView->setMarkerColor(color);
    });
    connect(penWidth, &QAction::triggered, this, [this]() {
        bool accepted = false;
        const qreal width = QInputDialog::getDouble(
            _window, tr("画笔粗细"), tr("像素"), _boardView->penWidth(),
            1.0, 24.0, 1, &accepted);
        if (accepted)
            _boardView->setPenWidth(width);
    });
    connect(markerWidth, &QAction::triggered, this, [this]() {
        bool accepted = false;
        const qreal width = QInputDialog::getDouble(
            _window, tr("荧光笔粗细"), tr("像素"), _boardView->markerWidth(),
            4.0, 48.0, 1, &accepted);
        if (accepted)
            _boardView->setMarkerWidth(width);
    });
    _brushButton->setMenu(menu);
    this->addWidget(_brushButton);
}

void EBDrawBar::createTextFormatMenu()
{
    _textFormatButton = new QToolButton(this);
    _textFormatButton->setObjectName(QStringLiteral("textFormatButton"));
    _textFormatButton->setIcon(ebToolbarIcon("text_format"));
    _textFormatButton->setText(tr("文本"));
    _textFormatButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    _textFormatButton->setToolTip(tr("文字格式"));
    _textFormatButton->setAccessibleName(tr("文字格式"));
    _textFormatButton->setPopupMode(QToolButton::InstantPopup);
    _textFormatButton->setEnabled(false);
    QMenu *menu = new QMenu(_textFormatButton);
    QAction *fontAction = menu->addAction(ebToolbarIcon("text_format"),
                                          tr("字体和字号..."));
    fontAction->setObjectName(QStringLiteral("textFontAction"));
    QAction *colorAction = menu->addAction(ebToolbarIcon("text_color"),
                                           tr("文字颜色..."));
    colorAction->setObjectName(QStringLiteral("textColorAction"));
    connect(fontAction, &QAction::triggered, this, [this]() {
        if (!_boardView->canFormatSelectedText())
            return;
        bool accepted = false;
        const QFont font = QFontDialog::getFont(
            &accepted, _boardView->selectedTextFont(), _window,
            tr("文字字体"));
        if (accepted)
            _boardView->setSelectedTextFont(font);
    });
    connect(colorAction, &QAction::triggered, this, [this]() {
        if (!_boardView->canFormatSelectedText())
            return;
        const QColor color = QColorDialog::getColor(
            _boardView->selectedTextColor(), _window, tr("文字颜色"));
        if (color.isValid())
            _boardView->setSelectedTextColor(color);
    });
    _textFormatButton->setMenu(menu);
    this->addWidget(_textFormatButton);
}
