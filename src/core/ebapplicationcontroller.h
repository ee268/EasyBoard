#ifndef EBAPPLICATIONCONTROLLER_H
#define EBAPPLICATIONCONTROLLER_H

#include <QObject>
#include <QPointer>

class EBMainWindow;

class EBApplicationController : public QObject
{
    Q_OBJECT
public:
    enum class MainMode {
        Board,
        Document,
        Web,
        Desktop
    };
    Q_ENUM(MainMode)

    explicit EBApplicationController(EBMainWindow* mainWindow, QObject *parent = nullptr);

    MainMode mainMode() const;

private:
    QPointer<EBMainWindow> _mainWindow;
    MainMode _mainMode;

public slots:
    void showMode(MainMode mode);
};

#endif // EBAPPLICATIONCONTROLLER_H
