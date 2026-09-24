#include "ebdrawbar.h"

#include <QAction>
#include <QMenu>
#include <QToolButton>

#include "../board/ebboardview.h"
#include "ebicons.h"

void EBDrawBar::createTeachingMenu()
{
    _teachingButton = new QToolButton(this);
    _teachingButton->setObjectName(QStringLiteral("teachingToolsButton"));
    _teachingButton->setIcon(ebToolbarIcon("teaching"));
    _teachingButton->setText(tr("教学工具"));
    _teachingButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    _teachingButton->setToolTip(tr("教学工具"));
    _teachingButton->setAccessibleName(tr("教学工具"));
    _teachingButton->setPopupMode(QToolButton::InstantPopup);
    QMenu *menu = new QMenu(_teachingButton);
    const QString labels[] = {tr("直尺"), tr("45° 三角板"), tr("30° 三角板"),
                              tr("量角器"), tr("圆规"), tr("幕布"),
                              tr("聚光灯"), tr("放大镜")};
    const char *names[] = {"teachingRulerAction", "teachingTriangle45Action",
                           "teachingTriangle30Action", "teachingProtractorAction",
                           "teachingCompassAction", "teachingCurtainAction",
                           "teachingSpotlightAction", "teachingMagnifierAction"};
    const char *icons[] = {"ruler", "triangle", "triangle", "protractor",
                           "compass", "curtain", "spotlight", "magnifier"};
    const EBTeachingTools::Kind kinds[] = {
        EBTeachingTools::Kind::Ruler, EBTeachingTools::Kind::Triangle45,
        EBTeachingTools::Kind::Triangle30, EBTeachingTools::Kind::Protractor,
        EBTeachingTools::Kind::Compass, EBTeachingTools::Kind::Curtain,
        EBTeachingTools::Kind::Spotlight, EBTeachingTools::Kind::Magnifier
    };
    for (int index = 0; index < 8; ++index) {
        QAction *action = menu->addAction(ebToolbarIcon(icons[index]),
                                          labels[index]);
        action->setObjectName(QString::fromLatin1(names[index]));
        connect(action, &QAction::triggered, _boardView,
                [this, kind = kinds[index]]() {
            _boardView->setTeachingTool(kind);
        });
    }
    menu->addSeparator();
    QAction *circle = menu->addAction(ebToolbarIcon("circle"), tr("圆规画整圆"));
    circle->setObjectName(QStringLiteral("teachingCircleAction"));
    connect(circle, &QAction::triggered, _boardView,
            &EBBoardView::drawCompassCircle);
    QAction *flipHorizontal = menu->addAction(
        ebToolbarIcon("triangle"), tr("三角板水平翻转"));
    flipHorizontal->setObjectName(QStringLiteral("teachingFlipHorizontalAction"));
    connect(flipHorizontal, &QAction::triggered, _boardView, [this]() {
        _boardView->flipTeachingTool(true);
    });
    QAction *flipVertical = menu->addAction(
        ebToolbarIcon("triangle"), tr("三角板垂直翻转"));
    flipVertical->setObjectName(QStringLiteral("teachingFlipVerticalAction"));
    connect(flipVertical, &QAction::triggered, _boardView, [this]() {
        _boardView->flipTeachingTool(false);
    });
    QAction *reset = menu->addAction(
        ebToolbarIcon("protractor"), tr("量角器归零"));
    reset->setObjectName(QStringLiteral("teachingResetAction"));
    connect(reset, &QAction::triggered, _boardView,
            &EBBoardView::resetTeachingTool);
    menu->addSeparator();
    QAction *magnifierZoomIn = menu->addAction(
        ebToolbarIcon("zoom_in"), tr("放大镜增加倍率"));
    magnifierZoomIn->setObjectName(QStringLiteral("teachingMagnifierZoomInAction"));
    connect(magnifierZoomIn, &QAction::triggered, _boardView, [this]() {
        _boardView->zoomTeachingMagnifier(0.5);
    });
    QAction *magnifierZoomOut = menu->addAction(
        ebToolbarIcon("zoom_out"), tr("放大镜降低倍率"));
    magnifierZoomOut->setObjectName(QStringLiteral("teachingMagnifierZoomOutAction"));
    connect(magnifierZoomOut, &QAction::triggered, _boardView, [this]() {
        _boardView->zoomTeachingMagnifier(-0.5);
    });
    QAction *magnifierShape = menu->addAction(
        ebToolbarIcon("magnifier"), tr("切换放大镜形状"));
    magnifierShape->setObjectName(QStringLiteral("teachingMagnifierShapeAction"));
    connect(magnifierShape, &QAction::triggered, _boardView,
            &EBBoardView::toggleTeachingMagnifierShape);
    menu->addSeparator();
    QAction *hide = menu->addAction(ebToolbarIcon("teaching"), tr("收起教具"));
    hide->setObjectName(QStringLiteral("teachingHideAction"));
    connect(hide, &QAction::triggered, _boardView, [this]() {
        _boardView->setTeachingTool(EBTeachingTools::Kind::None);
    });
    connect(menu, &QMenu::aboutToShow, this,
            [this, circle, flipHorizontal, flipVertical, reset,
             magnifierZoomIn, magnifierZoomOut, magnifierShape, hide]() {
        const EBTeachingTools::Kind kind = _boardView->teachingTool();
        circle->setEnabled(kind == EBTeachingTools::Kind::Compass);
        const bool triangle = kind == EBTeachingTools::Kind::Triangle45
            || kind == EBTeachingTools::Kind::Triangle30;
        flipHorizontal->setEnabled(triangle);
        flipVertical->setEnabled(triangle);
        reset->setEnabled(kind == EBTeachingTools::Kind::Protractor);
        const bool magnifier = kind == EBTeachingTools::Kind::Magnifier;
        magnifierZoomIn->setEnabled(magnifier);
        magnifierZoomOut->setEnabled(magnifier);
        magnifierShape->setEnabled(magnifier);
        hide->setEnabled(kind != EBTeachingTools::Kind::None);
    });
    _teachingButton->setMenu(menu);
    this->addWidget(_teachingButton);
}
