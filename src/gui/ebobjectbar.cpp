#include "ebobjectbar.h"

#include <QAction>
#include <QMainWindow>
#include <QMenu>
#include <QToolButton>

#include "../board/ebboardview.h"
#include "ebbarstyle.h"
#include "ebicons.h"

EBObjectBar::EBObjectBar(QMainWindow *window, EBBoardView *boardView,
                         const Actions &actions)
    : QToolBar(tr("对象操作"), window)
    , _window(window)
    , _boardView(boardView)
{
    setObjectName(QStringLiteral("objectToolBar"));
    window->addToolBar(Qt::RightToolBarArea, this);
    ebStyleBar(this, QStringLiteral("border-right: 1px solid #DCE6EA;"));
    const auto addMenuButton = [this](const char *icon, const QString &label,
                                      const char *name) {
        QToolButton *button = new QToolButton(this);
        button->setObjectName(QString::fromLatin1(name));
        button->setIcon(ebToolbarIcon(icon));
        button->setToolTip(label);
        button->setAccessibleName(label);
        button->setPopupMode(QToolButton::InstantPopup);
        addWidget(button);
        return button;
    };
    const auto setIcon = [](QAction *action, const char *icon,
                            const QString &tip) {
        action->setIcon(ebToolbarIcon(icon));
        action->setToolTip(tip);
    };
    setIcon(actions.cut, "cut", tr("剪切"));
    setIcon(actions.copy, "copy", tr("复制"));
    setIcon(actions.paste, "paste", tr("粘贴"));
    setIcon(actions.duplicate, "duplicate", tr("快速复制（Ctrl+D）"));
    QToolButton *clipboardButton = addMenuButton(
        "copy", tr("剪切、复制与粘贴"), "objectClipboardButton");
    QMenu *clipboardMenu = new QMenu(clipboardButton);
    for (QAction *action : {actions.cut, actions.copy,
                            actions.paste, actions.duplicate})
        clipboardMenu->addAction(action);
    clipboardButton->setMenu(clipboardMenu);

    setIcon(actions.group, "object_group", tr("组合（Ctrl+G）"));
    setIcon(actions.ungroup, "object_ungroup", tr("取消组合（Ctrl+Shift+G）"));
    actions.lock->setToolTip(tr("锁定对象（Ctrl+L）"));
    actions.unlock->setToolTip(tr("解锁对象（Ctrl+Shift+L）"));
    QToolButton *groupButton = addMenuButton(
        "object_group", tr("组合与锁定"), "objectGroupButton");
    QMenu *groupMenu = new QMenu(groupButton);
    groupMenu->addAction(actions.group);
    groupMenu->addAction(actions.ungroup);
    groupMenu->addSeparator();
    groupMenu->addAction(actions.lock);
    groupMenu->addAction(actions.unlock);
    groupButton->setMenu(groupMenu);

    _arrangeButton = addMenuButton(
        "object_arrange", tr("对齐与分布"), "arrangeObjectsButton");
    _arrangeButton->setEnabled(false);
    QMenu *arrangeMenu = new QMenu(_arrangeButton);
    for (int index = 0; index < 8; ++index) {
        if (index == 3 || index == 6)
            arrangeMenu->addSeparator();
        arrangeMenu->addAction(actions.arrangements[index]);
    }
    _arrangeButton->setMenu(arrangeMenu);

    const char *layerIcons[] = {"object_to_back", "object_backward",
                                "object_forward", "object_to_front"};
    QToolButton *layerButton = addMenuButton(
        "object_to_front", tr("对象层级"), "objectLayerButton");
    QMenu *layerMenu = new QMenu(layerButton);
    for (int index = 0; index < 4; ++index) {
        setIcon(actions.layers[index], layerIcons[index],
                actions.layers[index]->text());
        layerMenu->addAction(actions.layers[index]);
    }
    layerButton->setMenu(layerMenu);

    QToolButton *transformButton = addMenuButton(
        "object_scale_up", tr("对象变换与删除"), "objectTransformButton");
    QMenu *transformMenu = new QMenu(transformButton);
    createObjectActions(transformMenu);
    transformButton->setMenu(transformMenu);
    connect(_boardView, &EBBoardView::selectionAvailabilityChanged,
            this, &EBObjectBar::updateTransformActions);
    connect(_boardView, &EBBoardView::historyAvailabilityChanged,
            this, &EBObjectBar::updateTransformActions);
}

void EBObjectBar::setBoardActive(bool active)
{
    _boardModeActive = active;
    setVisible(active);
    updateTransformActions();
}

void EBObjectBar::setArrangeAvailable(bool available)
{
    _arrangeButton->setEnabled(available);
}

void EBObjectBar::updateTransformActions()
{
    const bool enabled = _boardModeActive
        && _boardView->hasEditableSelectedObjects();
    for (QAction *action : _objectActions)
        action->setEnabled(enabled);
}

void EBObjectBar::createObjectActions(QMenu *menu)
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
        QAction *action = menu->addAction(ebToolbarIcon(icons[index]),
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
    _window->addAction(_objectActions[4]);
    connect(_objectActions[4], &QAction::triggered,
            _boardView, &EBBoardView::deleteSelectedObject);
}
