#include "localpeer.h"

#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QCryptographicHash>
#include <QDir>
#include <QDataStream>

LocalPeer::LocalPeer(const QString &appId, QObject *parent)
    : QObject{parent}
    , _appId(appId)
    , _lockFile(nullptr)
    , _server(new QLocalServer(this))
    , _roleKnown(false)
    , _isSecondary(false)
{
    //哈希把appId转换为适合本地服务使用的稳定短名称
    const QByteArray digest =
        QCryptographicHash::hash(appId.toUtf8(), QCryptographicHash::Sha1).toHex();

    _socketName = QStringLiteral("easyboard-") + QString::fromLatin1(digest.left(16));

    //拼接锁文件的绝对路径，.lock是自定义的后缀命名约定，不是固定要求
    //temp表示系统临时目录
    const QString lockPath = QDir::temp().absoluteFilePath(_socketName + QStringLiteral(".lock"));
    _lockFile = new QLockFile(lockPath);

    connect(_server, &QLocalServer::newConnection,
            this, &LocalPeer::receiveConnections);
}

LocalPeer::~LocalPeer()
{
    delete _lockFile;
}

bool LocalPeer::isSecondary()
{
    if (_roleKnown)
        return _isSecondary;

    _roleKnown = true;
    if (_lockFile->tryLock(0)) {
        _isSecondary = true;
        return true;
    }

    //取得锁的进程是主实例；先移除异常退出可能遗留的本地服务端点。
    QLocalServer::removeServer(_socketName);
    if (!_server->listen(_socketName))
        qWarning() << "无法监听EasyBoard本地服务: " << _server->errorString();

    _isSecondary = false;
    return false;
}

bool LocalPeer::sendMessage(const QString &message, int timeout)
{
    //主实例不向自身发消息
    if (!isSecondary()) {
        return false;
    }

    QLocalSocket socket;
    socket.connectToServer(_socketName);
    if (!socket.waitForConnected(timeout))
        return false;

    QByteArray payload;
    QDataStream output(&payload, QIODevice::WriteOnly);
    output << message;
    socket.write(payload);

    //waitForBytesWritten等待有数据写向连接
    //waitForReadyRead等待有数据返回
    //两者有一个失败都代表消息发送失败
    if (!socket.waitForBytesWritten(timeout) ||
        !socket.waitForReadyRead(timeout))
    {
        return false;
    }

    return socket.readAll() == QByteArrayLiteral("ack");
}

QString LocalPeer::applicationId() const
{
    return _appId;
}

void LocalPeer::receiveConnections()
{
    //循环查询是否有连接在等待
    while (_server->hasPendingConnections()) {
        QLocalSocket* socket = _server->nextPendingConnection();
        if (!socket) {
            continue;
        }

        //如果数据尚未到达，最多等待两秒
        if (socket->bytesAvailable() == 0) {
            socket->waitForReadyRead(2000);
        }

        QDataStream input(socket); //读出数据到QString
        QString message;
        input >> message;

        //先返回确认，让第二个进程可以结束，再把消息交给应用层
        socket->write(QByteArrayLiteral("ack"));
        socket->waitForBytesWritten(1000); //等待数据写向连接

        socket->disconnectFromServer();
        socket->deleteLater();

        emit messageReceived(message);
    }
}


