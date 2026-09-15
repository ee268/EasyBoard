#ifndef EBAPPLICATION_H
#define EBAPPLICATION_H

#include <QApplication>
#include <QPointer>

class EBMainWindow;

class EBApplication : public QApplication
{
    Q_OBJECT
public:
    explicit EBApplication(const QString& id, int& argc, char **argv);
    ~EBApplication() override;

    int exec(const QString& fileToImport);

    void cleanup();

    static EBApplication* app();

public:
    static QPointer<EBMainWindow> _mainWindow;

private:
    QString _appId;
};

#endif // EBAPPLICATION_H
