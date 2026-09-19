#include "ebapplication.h"

#include <QDebug>
#include <QFileInfo>

#include "ebsettings.h"
#include "ebapplicationcontroller.h"
#include "../gui/ebmainwindow.h"
#include "../gui/ebresources.h"

QPointer<EBMainWindow> EBApplication::_mainWindow = nullptr;

EBApplication::EBApplication(const QString &id, int &argc, char **argv)
    : SingleApplication(id, argc, argv)
{
    setOrganizationName("ee268");
    setOrganizationDomain("ee268.cn");
    setApplicationName("EasyBoard");
    setApplicationVersion("0.0.2");

    const QIcon icon = EBResources::resources()->appIcon();
    setWindowIcon(icon);

    EBSettings::settings();

    //这些命令行形式都开启文件日志记录。
    const QStringList args = arguments();
    _isVerbose = args.contains(QStringLiteral("-v"))
                 || args.contains(QStringLiteral("-verbose"))
                 || args.contains(QStringLiteral("verbose"))
                 || args.contains(QStringLiteral("-log"))
                 || args.contains(QStringLiteral("log"));

    connect(this, &SingleApplication::messageReceived,
            this, &EBApplication::handleInstanceMessage);
}

EBApplication::~EBApplication()
{
    cleanup();
    EBSettings::destroy();
}

int EBApplication::exec(const QString &fileToImport)
{
    _mainWindow = new EBMainWindow;
    _appController = new EBApplicationController(_mainWindow, this);

    connect(_mainWindow, &EBMainWindow::fileImportRequested,
            this, &EBApplication::handleImportFileRequest);

    connect(_mainWindow, &EBMainWindow::quitRequested,
            this, &EBApplication::closing);

    if (!fileToImport.isEmpty()) {
        handleImportFileRequest(fileToImport);
    }

    _mainWindow->show();

    setActivationWindow(_mainWindow);

    return QApplication::exec();
}

void EBApplication::cleanup()
{
    if (_mainWindow) {
        //先保存当前窗口尺寸，再销毁窗口。下一次启动由 MainWindow 恢复
        EBSettings* settings = EBSettings::settings();
        settings->setWindowGeometry(_mainWindow->saveGeometry());
        if (!settings->save())
            qWarning() << "无法保存窗口Geometry设置";
    }

    delete _mainWindow.data();
}

EBApplication *EBApplication::app()
{
    // qApp为全局唯一创建的QApplication类
    // 当有application创建时自动赋值到qApp = &app
    return qobject_cast<EBApplication*>(qApp);
}

bool EBApplication::isVerbose() const
{
    return _isVerbose;
}

void EBApplication::closing()
{
    quit();
}

void EBApplication::handleInstanceMessage(const QString &message)
{
    if (!_mainWindow)
        return;

    if (message == QStringLiteral("__eboard_activate__"))
        qDebug() << "__eboard_activate__";
    else {
        qDebug() << tr("收到待打开文件：%1").arg(message);
        handleImportFileRequest(message);
    }
}

void EBApplication::handleImportFileRequest(const QString &filePath)
{
    if (!_mainWindow || filePath.isEmpty()) {
        return;
    }

    const QFileInfo file(filePath);
    if (!file.isFile()) {
        return;
    }
    _mainWindow->openDocument(file.absoluteFilePath());
}


