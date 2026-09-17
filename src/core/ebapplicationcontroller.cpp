#include "ebapplicationcontroller.h"

#include "../gui/ebmainwindow.h"

#include <QDebug>

EBApplicationController::EBApplicationController(EBMainWindow *mainWindow, QObject *parent)
    : QObject{parent}
    , _mainWindow(mainWindow)
    , _mainMode(MainMode::Board)
{
    connect(mainWindow, &EBMainWindow::modeRequested,
            this, &EBApplicationController::showMode);

    mainWindow->showMode(_mainMode);
}

EBApplicationController::MainMode EBApplicationController::mainMode() const
{
    return _mainMode;
}

void EBApplicationController::showMode(MainMode mode)
{
    if (!_mainWindow || _mainMode == mode)
        return;

    _mainMode = mode;
    _mainWindow->showMode(mode);

    qDebug() << "切换工作模式: " << mode;
}
