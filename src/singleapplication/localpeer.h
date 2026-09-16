#ifndef LOCALPEER_H
#define LOCALPEER_H

#include <QObject>

class QLockFile;
class QLocalServer;

class LocalPeer : public QObject
{
    Q_OBJECT
public:
    explicit LocalPeer(const QString& appId, QObject *parent = nullptr);
    ~LocalPeer() override;

    //判断当前进程是否为后启动实例
    bool isSecondary();

    //向主实例发送消息，timeout毫秒内等待ack确认
    bool sendMessage(const QString& message, int timeout);

    QString applicationId() const;

private:
    QString _appId;
    QString _socketName;

    QLockFile* _lockFile;
    QLocalServer* _server;

    bool _roleKnown;
    bool _isSecondary;

signals:
    //主实例完成消息读取并完成确认
    void onMessageReceived(const QString& message);

private slots:
    //处理_server的待连接队列
    void receiveConnections();
};

#endif // LOCALPEER_H
