#include "ebapplication.h"
#include "ebsettings.h"
#include "../gui/ebdialoglocalizer.h"

#include <QFile>
#include <QTranslator>
#include <QDebug>
#include <QDebug>
#include <QDir>
#include <QDateTime>

void ebMessageOutput(QtMsgType type, const QMessageLogContext &context,
                     const QString &message)
{
    //先取消安装，避免无限递归自身
    QtMessageHandler preHandler = qInstallMessageHandler(nullptr);

#if defined(QT_NO_DEBUG) //判断构建时是否非debug模式，是则输出日志时忽略所有qDebug
    if (type != QtDebugMsg)
        qt_message_output(type, context, message);
#else
    qt_message_output(type, context, message);
#endif

    auto app = EBApplication::app();
    if (app && app->isVerbose()) {
        const QString path = QDir(EBSettings::logDir()).absoluteFilePath(
            app->applicationName() + QStringLiteral(".log"));

        QFile logFile(path);

        //超过10MB时删除重建
        if (logFile.exists() && logFile.size() > 10000000)
            logFile.remove();

        if (logFile.open(QIODevice::Append | QIODevice::Text)) {
            QTextStream output(&logFile);
            output.setCodec("UTF-8");
            output << QDateTime::currentDateTime().toString(Qt::ISODate)
                   << "       " << message << '\n';
        }
    }

    qInstallMessageHandler(preHandler);
}

int main(int argc, char *argv[])
{
    //移除可能一项linux多媒体支持的环境变量
    if (qEnvironmentVariableIsSet("QT_NO_GLIB"))
        qunsetenv("QT_NO_GLIB");

    //安装消息处理器，处理qDebug qWarning qInfo等
    qInstallMessageHandler(ebMessageOutput);

    EBApplication app("EasyBoard", argc, argv);
    QTranslator qtTranslator;
    if (qtTranslator.load(QStringLiteral(":/translations/qt_zh_CN.qm")))
        app.installTranslator(&qtTranslator);
    EBDialogLocalizer dialogLocalizer;
    app.installEventFilter(&dialogLocalizer);

    const QStringList arguments = app.arguments();

    QString fileToOpen;
    if (arguments.size() > 1) {
        const QFile candidate(arguments.at(1));
        if (candidate.exists())
            fileToOpen = candidate.fileName();
    }

    //后启动实例把用户意图交给主实例后立即退出，不再创建第二个窗口
    if (app.isSecondary()) {
        qDebug() << "current application is secondary process";
        const QString message =
            fileToOpen.isEmpty() ? QStringLiteral("__eboard_activate__") : fileToOpen;

        //发送成功则 0正常退出， 1错误
        return app.sendMessage(message) ? 0 : 1;
    }

    qDebug() << "待打开文件: " << fileToOpen;
    const int result = app.exec(fileToOpen);

    app.cleanup();
    qDebug() << "EasyBoard 应用退出";

    return result;
}
