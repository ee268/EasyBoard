#include "ebmainwindow.h"

EBMainWindow::EBMainWindow(const QString &fileToOpen, QWidget *parent)
    : QMainWindow{parent}
{
    resize(960, 640);
    setWindowTitle(tr("EasyBoard"));
}
