#ifndef EBAPPLICATION_H
#define EBAPPLICATION_H

#include "../singleapplication/singleapplication.h"
#include <QPointer>

class EBMainWindow;

class EBApplication : public SingleApplication
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

private slots:
    void handleInstanceMessage(const QString& message);
};

#endif // EBAPPLICATION_H
