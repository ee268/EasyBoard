#ifndef EBMAINWINDOW_H
#define EBMAINWINDOW_H

#include <QMainWindow>

#include "../core/ebapplicationcontroller.h"
#include "../domain/ebdocument.h"

class QStackedWidget;
class EBBoardView;
class EBCommandController;
class EBDocumentLibrary;
class EBPageNavigator;

class EBMainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit EBMainWindow(QWidget *parent = nullptr);

    void showMode(EBApplicationController::MainMode mode);
    bool openDocument(const QString &path);
    bool importImage(const QString &path);
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

    EBDocument _document;
    QStackedWidget *_modeStack;
    QWidget *_boardWorkspace;
    EBBoardView *_boardView;
    EBPageNavigator *_pageNavigator;
    EBDocumentLibrary *_documentLibrary;
    EBCommandController *_commandController;

signals:
    void fileImportRequested(const QString &path);
    void quitRequested();
    void modeRequested(EBApplicationController::MainMode mode);
};

#endif // EBMAINWINDOW_H
