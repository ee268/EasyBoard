#ifndef EBMAINWINDOW_H
#define EBMAINWINDOW_H

#include <QMainWindow>

#include "../core/ebapplicationcontroller.h"

class QStackedWidget;

class EBMainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit EBMainWindow(QWidget *parent = nullptr);

    void showMode(EBApplicationController::MainMode mode);

private:
    QStackedWidget* _modeStack;

signals:
    void fileImportRequested(const QString& path);

    void quitRequested();

    void modeRequested(EBApplicationController::MainMode mode);
};

#endif // EBMAINWINDOW_H
