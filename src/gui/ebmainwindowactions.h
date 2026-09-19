#ifndef EBMAINWINDOWACTIONS_H
#define EBMAINWINDOWACTIONS_H

#include <QObject>

#include "../core/ebapplicationcontroller.h"

class QAction;
class QMainWindow;
class QToolBar;
class QToolButton;
class EBBoardView;

// 主窗口的菜单、工具栏和动作连接集中在此，窗口自身只维护页面布局。
class EBMainWindowActions : public QObject
{
    Q_OBJECT

public:
    EBMainWindowActions(QMainWindow *window, EBBoardView *boardView);

    void setMode(EBApplicationController::MainMode mode);
    QString modeLabel(EBApplicationController::MainMode mode) const;

signals:
    void fileImportRequested(const QString &filePath);
    void saveDocumentRequested();
    void quitRequested();
    void modeRequested(EBApplicationController::MainMode mode);

private:
    void createFileMenu();
    void createToolBar();
    void createBackgroundMenu(QToolBar *toolBar);
    void createZoomActions(QToolBar *toolBar);
    void createDrawingActions(QToolBar *toolBar);
    void createModeActions(QToolBar *toolBar);

    QMainWindow *_window;
    EBBoardView *_boardView;
    QToolButton *_backgroundButton = nullptr;
    QAction *_undoAction = nullptr;
    QAction *_redoAction = nullptr;
    QAction *_zoomInAction = nullptr;
    QAction *_zoomOutAction = nullptr;
    QAction *_fitPageAction = nullptr;
    QAction *_colorActions[2] = {};
    QAction *_patternActions[3] = {};
    QAction *_sizeActions[2] = {};
    QAction *_toolActions[6] = {};
    QAction *_modeActions[4] = {};
};

#endif
