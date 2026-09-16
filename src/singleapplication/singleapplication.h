#ifndef SINGLEAPPLICATION_H
#define SINGLEAPPLICATION_H

#include <QApplication>

class LocalPeer;

class SingleApplication : public QApplication
{
    Q_OBJECT
public:
    explicit SingleApplication(const QString& appId, int& argc, char** argv);

    //判断当前进程是否为后启动实例
    bool isSecondary();

    bool sendMessage(const QString& message, int timeout = 5000);

    void setActivationWindow(QWidget* window, bool activateOnMsg = true);

    QWidget* activationWindow() const;

    QString id() const;

private:
    LocalPeer* _peer;
    QWidget* _activationWindow;

public slots:
    //将主实例窗口提升为活动窗口
    void activateWindow();

signals:
    void messageReceived(const QString& message);
};

#endif // SINGLEAPPLICATION_H
