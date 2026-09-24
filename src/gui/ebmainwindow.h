#ifndef EBMAINWINDOW_H
#define EBMAINWINDOW_H

#include <QMainWindow>

#include "../core/ebapplicationcontroller.h"
#include "../domain/ebdocument.h"

class QStackedWidget;
class EBBoardView;
class EBCommands;
class EBDocumentLibrary;
class EBPagePanel;
class EBDisplayView;
class EBWebWorkspace;
class EBDesktopOverlay;
class QImage;

class EBMainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit EBMainWindow(QWidget *parent = nullptr);
    ~EBMainWindow() override;

    void showMode(EBApplicationController::MainMode mode);
    bool openDocument(const QString &path);
    bool importImage(const QString &path);
    bool importPdf(const QString &path);
    bool insertImageObject(const QString &path);
    bool importDocumentPackage(const QString &path);

private:
    void newDocument();
    void duplicateDocument(const QString &path);
    void saveDocument();
    void exportCurrentPageImage();
    void exportDocumentPdf();
    void exportDocumentPackage();
    bool prepareForImportedDocument();
    bool activateImportedDocument(const EBDocument &document,
                                  QString *savedPath, QString *error);
    bool saveCurrentDocument(bool showMessage);
    bool loadDocument(const QString &path, bool switchToBoard);
    void restoreLastDocument();
    void refreshDocumentLibrary();
    void renameDocument(const QString &path, const QString &title);
    void moveDocumentToTrash(const QString &path);
    void restoreDocument(const QString &path);
    void deleteDocument(const QString &path);
    void resetCurrentDocument();
    void setDisplayVisible(bool visible);
    void ensureWebWorkspace();
    void ensureDesktopOverlay();
    void insertCapturedWebImage(const QImage &image);
    void insertCapturedDesktopImage(const QImage &image);
    bool insertCapturedImage(const QImage &image, const QString &source);

    EBDocument _document;
    QStackedWidget *_modeStack;
    QWidget *_boardWorkspace;
    EBBoardView *_boardView;
    EBPagePanel *_pagePanel;
    EBDocumentLibrary *_documentLibrary;
    EBCommands *_commands;
    EBDisplayView *_displayView = nullptr;
    QWidget *_webPlaceholder = nullptr;
    EBWebWorkspace *_webWorkspace = nullptr;
    EBDesktopOverlay *_desktopOverlay = nullptr;

signals:
    void fileImportRequested(const QString &path);
    void quitRequested();
    void modeRequested(EBApplicationController::MainMode mode);
};

#endif // EBMAINWINDOW_H
