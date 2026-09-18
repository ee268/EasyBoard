#ifndef EBMAINWINDOW_H
#define EBMAINWINDOW_H

#include <QMainWindow>

#include "../core/ebapplicationcontroller.h"
#include "../domain/ebdocument.h"

class QStackedWidget;
class EBBoardView;
class EBMainWindowActions;
class EBPageNavigator;

class EBMainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit EBMainWindow(QWidget *parent = nullptr);

    void showMode(EBApplicationController::MainMode mode);

private:
    EBDocument _document;
    QStackedWidget *_modeStack;
    QWidget *_boardWorkspace;
    EBBoardView *_boardView;
    EBPageNavigator *_pageNavigator;
    EBMainWindowActions *_actions;

signals:
    void fileImportRequested(const QString &path);
    void quitRequested();
    void modeRequested(EBApplicationController::MainMode mode);
};

#endif // EBMAINWINDOW_H
