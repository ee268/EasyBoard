#ifndef EBTOPBAR_H
#define EBTOPBAR_H

#include <QToolBar>

#include "../core/ebapplicationcontroller.h"

class QAction;
class QMainWindow;
class QToolButton;
class EBBoardView;

// 顶部只保留工作区入口、页面栏开关和全局白板命令。
class EBTopBar : public QToolBar
{
    Q_OBJECT
public:
    EBTopBar(QMainWindow *window, EBBoardView *boardView,
             QAction *insertImageAction);

    void setMode(EBApplicationController::MainMode mode);
    QString modeLabel(EBApplicationController::MainMode mode) const;
    void setDisplayVisible(bool visible);

signals:
    void modeRequested(EBApplicationController::MainMode mode);
    void pagePanelVisibilityRequested(bool visible);
    void displayViewRequested(bool visible);

private:
    void createModeActions();
    void createBackgroundMenu();
    void createZoomActions();
    void syncPage();

    EBBoardView *_boardView;
    QAction *_insertImageAction;
    QAction *_pagePanelAction = nullptr;
    QAction *_displayAction = nullptr;
    QAction *_undoAction = nullptr;
    QAction *_redoAction = nullptr;
    QAction *_zoomInAction = nullptr;
    QAction *_zoomOutAction = nullptr;
    QAction *_fitPageAction = nullptr;
    QToolButton *_backgroundButton = nullptr;
    QAction *_colorActions[2] = {};
    QAction *_patternActions[3] = {};
    QAction *_sizeActions[3] = {};
    QAction *_modeActions[4] = {};
    bool _boardModeActive = false;
};

#endif
