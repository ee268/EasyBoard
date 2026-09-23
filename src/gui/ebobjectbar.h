#ifndef EBOBJECTBAR_H
#define EBOBJECTBAR_H

#include <QToolBar>

class QAction;
class QMainWindow;
class QMenu;
class QToolButton;
class EBBoardView;

// 侧边栏组织对象菜单；编辑菜单与侧边栏复用同一组动作。
class EBObjectBar : public QToolBar
{
    Q_OBJECT
public:
    struct Actions {
        QAction *cut;
        QAction *copy;
        QAction *paste;
        QAction *duplicate;
        QAction *group;
        QAction *ungroup;
        QAction *lock;
        QAction *unlock;
        QAction *layers[4];
        QAction *arrangements[8];
    };

    EBObjectBar(QMainWindow *window, EBBoardView *boardView,
                const Actions &actions);

    void setBoardActive(bool active);
    void setArrangeAvailable(bool available);
    void updateTransformActions();

private:
    void createObjectActions(QMenu *menu);

    QMainWindow *_window;
    EBBoardView *_boardView;
    QToolButton *_arrangeButton = nullptr;
    QAction *_objectActions[5] = {};
    bool _boardModeActive = false;
};

#endif
