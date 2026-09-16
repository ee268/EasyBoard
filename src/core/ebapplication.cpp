#include "ebapplication.h"

#include "../gui/ebmainwindow.h"

EBApplication::EBApplication(const QString &id, int &argc, char **argv)
    : SingleApplication(id, argc, argv)
{
    setOrganizationName("ee268");
    setOrganizationDomain("ee268.cn");
    setApplicationName("EasyBoard");
    setApplicationVersion("0.0.2");

    connect(this, &SingleApplication::messageReceived,
            this, &EBApplication::handleInstanceMessage);
}

EBApplication::~EBApplication()
{
    cleanup();
}

int EBApplication::exec(const QString &fileToImport)
{
    _mainWindow = new EBMainWindow(fileToImport);
    _mainWindow->show();

    setActivationWindow(_mainWindow);

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

void EBApplication::handleInstanceMessage(const QString &message)
{
    if (!_mainWindow)
        return;

    // if (message == QStringLiteral("__wboard_activate__"))
    //     mainWindow->showIncomingMessage(tr("收到另一个 WBoard 启动请求"));
    // else
    //     mainWindow->showIncomingMessage(tr("收到待打开文件：%1").arg(message));
}
