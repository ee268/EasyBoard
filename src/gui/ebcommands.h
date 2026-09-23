#ifndef EBCOMMANDS_H
#define EBCOMMANDS_H

#include <QObject>

#include "../core/ebapplicationcontroller.h"

class QAction;
class QMainWindow;
class EBBoardView;
class EBTopBar;
class EBDrawBar;
class EBObjectBar;

// 文件和编辑菜单复用同一组动作，并协调各功能区的模式状态。
class EBCommands : public QObject
{
    Q_OBJECT
public:
    EBCommands(QMainWindow *window, EBBoardView *boardView);

    void setMode(EBApplicationController::MainMode mode);
    QString modeLabel(EBApplicationController::MainMode mode) const;

signals:
    void newDocumentRequested();
    void fileImportRequested(const QString &filePath);
    void imageObjectInsertRequested(const QString &filePath);
    void saveDocumentRequested();
    void exportPageImageRequested();
    void exportDocumentPdfRequested();
    void exportDocumentPackageRequested();
    void quitRequested();
    void modeRequested(EBApplicationController::MainMode mode);
    void pagePanelVisibilityRequested(bool visible);

private:
    void createFileMenu();
    void createEditMenu();
    void refreshActions();
    void updateClipboardActions();
    void updateLayerActions();
    void updateGroupActions();
    void updateLockActions();
    void updateArrangeActions();
    void updateSelectAllAction();

    QMainWindow *_window;
    EBBoardView *_boardView;
    EBTopBar *_topBar;
    EBDrawBar *_drawBar;
    EBObjectBar *_objectBar;
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
    QAction *_layerActions[4] = {};
    QAction *_arrangeActions[8] = {};
    bool _boardModeActive = false;
};

#endif
