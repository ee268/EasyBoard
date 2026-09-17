#ifndef EBAPPLICATION_H
#define EBAPPLICATION_H

#include "../singleapplication/singleapplication.h"
#include <QPointer>

class EBMainWindow;
class EBApplicationController;

class EBApplication : public SingleApplication
{
    Q_OBJECT
public:
    explicit EBApplication(const QString& id, int& argc, char **argv);
    ~EBApplication() override;

    int exec(const QString& fileToImport);

    void cleanup();

    static EBApplication* app();

    bool isVerbose() const;

    void closing();

public:
    static QPointer<EBMainWindow> _mainWindow;

private:
    //只保存本次启动是否启用文件日志，不负责日志文件的具体写入
    bool _isVerbose;

    EBApplicationController* _appController;

private slots:
    void handleInstanceMessage(const QString& message);

    void handleImportFileRequest(const QString& filePath);
};

#endif // EBAPPLICATION_H
