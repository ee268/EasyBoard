#ifndef EBCOMMANDCONTROLLER_H
#define EBCOMMANDCONTROLLER_H

#include <QObject>

#include "../core/ebapplicationcontroller.h"

class QAction;
class QIcon;
class QMainWindow;
class QToolBar;
class QToolButton;
class EBBoardView;

// 协调主窗口命令、菜单与工具栏状态，窗口自身只维护页面布局。
class EBCommandController : public QObject
{
    Q_OBJECT

public:
    EBCommandController(QMainWindow *window, EBBoardView *boardView);

    void setMode(EBApplicationController::MainMode mode);
    QString modeLabel(EBApplicationController::MainMode mode) const;

signals:
    void fileImportRequested(const QString &filePath);
    void imageObjectInsertRequested(const QString &filePath);
    void saveDocumentRequested();
    void exportPageImageRequested();
    void exportDocumentPdfRequested();
    void exportDocumentPackageRequested();
    void quitRequested();
    void modeRequested(EBApplicationController::MainMode mode);

private:
    static QIcon toolbarIcon(const char *name);
    void createFileMenu();
    void createEditMenu();
    void createToolBar();
    void createBackgroundMenu(QToolBar *toolBar);
    void createZoomActions(QToolBar *toolBar);
    void createDrawingActions(QToolBar *toolBar);
    void createObjectActions(QToolBar *toolBar);
    void createModeActions(QToolBar *toolBar);
    void updateObjectActions();
    void updateClipboardActions();
    void updateLayerActions();
    void updateGroupActions();
    void updateLockActions();
    void updateArrangeActions();
    void updateSelectAllAction();

    QMainWindow *_window;
    EBBoardView *_boardView;
    QToolButton *_backgroundButton = nullptr;
    QToolButton *_arrangeButton = nullptr;
    QAction *_undoAction = nullptr;
    QAction *_redoAction = nullptr;
    QAction *_zoomInAction = nullptr;
    QAction *_zoomOutAction = nullptr;
    QAction *_fitPageAction = nullptr;
    QAction *_insertImageObjectAction = nullptr;
    QAction *_cutAction = nullptr;
    QAction *_copyAction = nullptr;
    QAction *_pasteAction = nullptr;
    QAction *_duplicateAction = nullptr;
    QAction *_selectAllAction = nullptr;
    QAction *_groupAction = nullptr;
    QAction *_ungroupAction = nullptr;
    QAction *_lockAction = nullptr;
    QAction *_unlockAction = nullptr;
    QAction *_snapAction = nullptr;
    QAction *_gridSnapAction = nullptr;
    QAction *_colorActions[2] = {};
    QAction *_patternActions[3] = {};
    QAction *_sizeActions[2] = {};
    QAction *_toolActions[8] = {};
    QAction *_objectActions[5] = {};
    QAction *_layerActions[4] = {};
    QAction *_arrangeActions[8] = {};
    QAction *_modeActions[4] = {};
    bool _boardModeActive = false;
};

#endif
