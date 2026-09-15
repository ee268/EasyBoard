#include "ebapplication.h"

#include "../gui/ebmainwindow.h"

EBApplication::EBApplication(const QString &id, int &argc, char **argv)
    : QApplication(argc, argv)
    , _appId(id)
{
    setOrganizationName("ee268");
    setOrganizationDomain("ee268.cn");
    setApplicationName("EasyBoard");
    setApplicationVersion("0.0.1");
}

EBApplication::~EBApplication()
{
    cleanup();
}

int EBApplication::exec(const QString &fileToImport)
{
    _mainWindow = new EBMainWindow(fileToImport);
    _mainWindow->show();

    return QApplication::exec();
}

void EBApplication::cleanup()
{
    delete _mainWindow;
}

EBApplication *EBApplication::app()
{
    // qApp为全局唯一创建的QApplication类
    // 当有application创建时自动赋值到qApp = &app
    return qobject_cast<EBApplication*>(qApp);
}
