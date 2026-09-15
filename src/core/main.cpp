#include "ebapplication.h"

#include <QFile>
#include <QDebug>

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
    const QStringList arguments = app.arguments();

    QString fileToOpen;
    if (arguments.size() > 1) {
        const QFile candidate(arguments.at(1));
        if (candidate.exists())
            fileToOpen = candidate.fileName();
    }

    qDebug() << "待打开文件: " << fileToOpen;
    const int result = app.exec(fileToOpen);

    app.cleanup();
    qDebug() << "EasyBoard 应用退出";

    return result;
}
