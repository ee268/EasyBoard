#include "singleapplication.h"

#include "localpeer.h"

#include <QWidget>

#include <QDebug>

SingleApplication::SingleApplication(const QString &appId, int &argc, char **argv)
    : QApplication{argc, argv}
    , _peer(new LocalPeer(appId, this))
    , _activationWindow(nullptr)
{
    connect(_peer, &LocalPeer::messageReceived,
            this, &SingleApplication::messageReceived);
}

bool SingleApplication::isSecondary()
{
    return _peer->isSecondary();
}

bool SingleApplication::sendMessage(const QString &message, int timeout/* = 5000*/)
{
    return _peer->sendMessage(message, timeout);
}

void SingleApplication::setActivationWindow(QWidget *window, bool activateOnMsg/* = true */)
{
    _activationWindow = window;

    disconnect(_peer, &LocalPeer::messageReceived,
               this, &SingleApplication::activateWindow);

    if (activateOnMsg) {
        connect(_peer, &LocalPeer::messageReceived,
                this, &SingleApplication::activateWindow, Qt::UniqueConnection);
    }
}

QWidget *SingleApplication::activationWindow() const
{
    return _activationWindow;
}

QString SingleApplication::id() const
{
    return _peer->applicationId();
}

void SingleApplication::activateWindow()
{
    if (!_activationWindow)
        return;

    qDebug() << "activate window";

    //移除窗口最小化，显示并将窗口提升到最前且激活
    _activationWindow->setWindowState(
        _activationWindow->windowState() & ~Qt::WindowMinimized);

    _activationWindow->show();
    _activationWindow->raise();
    _activationWindow->activateWindow();
}
