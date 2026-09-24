#include "ebmainwindow.h"

#include "../board/ebboardview.h"
#include "../core/ebsettings.h"
#include "../export/ebdocumentexporter.h"
#include "../import/ebimageimporter.h"
#include "../import/ebpdfimporter.h"
#include "../persistence/ebdocumentpackage.h"
#include "../persistence/ebdocumentstorage.h"
#include "ebdocumentlibrary.h"
#include "ebcommands.h"
#include "ebpagepanel.h"
#include "ebdisplayview.h"

#include <QDebug>
#include <QHBoxLayout>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QEventLoop>
#include <QProgressDialog>
#include <QStackedWidget>
#include <QStatusBar>
#include <QThread>

namespace {
bool documentHasContent(const EBDocument &document)
{
    if (document.pageCount() > 1)
        return true;
    for (int index = 0; index < document.pageCount(); ++index) {
        const EBPage *page = document.pageAt(index);
        if (!page->strokes().isEmpty() || !page->texts().isEmpty()
            || !page->images().isEmpty() || !page->teachingTools().isEmpty()
            || page->hasBackgroundImage()
            || !page->horizontalGuides().isEmpty()
            || !page->verticalGuides().isEmpty()
            || page->color() != EBPage::Color::White
            || page->pattern() != EBPage::Pattern::Blank
            || page->size() != EBPage::Size::Standard)
            return true;
    }
    return false;
}

QString exportBaseName(QString title)
{
    const QString invalid = QStringLiteral("\\/:*?\"<>|");
    for (int index = 0; index < title.size(); ++index) {
        if (invalid.contains(title.at(index)))
            title[index] = QLatin1Char('_');
    }
    title = title.trimmed();
    return title.isEmpty() ? QStringLiteral("EasyBoard") : title;
}
}

EBMainWindow::EBMainWindow(QWidget *parent)
    : QMainWindow{parent}
    , _modeStack(new QStackedWidget(this))
    , _boardWorkspace(new QWidget(_modeStack))
    , _boardView(new EBBoardView(&_document, _boardWorkspace))
    , _pagePanel(new EBPagePanel(&_document, _boardWorkspace))
    , _documentLibrary(new EBDocumentLibrary(_modeStack))
    , _commands(nullptr)
{
    setWindowTitle(tr("EasyBoard"));

    const QByteArray geometry = EBSettings::settings()->windowGeometry();
    if (geometry.isEmpty() || !restoreGeometry(geometry))
        resize(960, 640);
    qDebug() << "主窗口尺寸: " << size();

    // 白板左侧显示文档页面，主区域仍由画板视图负责绘制。
    QHBoxLayout *boardLayout = new QHBoxLayout(_boardWorkspace);
    boardLayout->setContentsMargins(0, 0, 0, 0);
    boardLayout->setSpacing(0);
    boardLayout->addWidget(_pagePanel);
    boardLayout->addWidget(_boardView, 1);
    _modeStack->addWidget(_boardWorkspace);
    connect(_pagePanel, &EBPagePanel::pageSelected,
            _boardView, &EBBoardView::setCurrentPageIndex);
    connect(_pagePanel, &EBPagePanel::addPageRequested,
            _boardView, &EBBoardView::addPage);
    connect(_pagePanel, &EBPagePanel::duplicatePageRequested,
            _boardView, &EBBoardView::duplicateCurrentPage);
    connect(_pagePanel, &EBPagePanel::removePageRequested,
            _boardView, &EBBoardView::removeCurrentPage);
    connect(_pagePanel, &EBPagePanel::movePageRequested,
            _boardView, &EBBoardView::moveCurrentPage);
    connect(_boardView, &EBBoardView::pageListChanged,
            _pagePanel, &EBPagePanel::refreshPages);
    connect(_boardView, &EBBoardView::currentPageChanged,
            _pagePanel, &EBPagePanel::setCurrentPageIndex);
    connect(_boardView, &EBBoardView::pageContentChanged,
            _pagePanel, &EBPagePanel::refreshPage);
    connect(_boardView, &EBBoardView::pagePositionChanged,
            this, [this](const QPointF &pagePosition, bool insidePage) {
        if (insidePage) {
            statusBar()->showMessage(tr("页面坐标：(%1, %2)")
                                         .arg(qRound(pagePosition.x()))
                                         .arg(qRound(pagePosition.y())));
        } else {
            statusBar()->clearMessage();
        }
    });

    _modeStack->addWidget(_documentLibrary);
    connect(_documentLibrary, &EBDocumentLibrary::openRequested,
            this, [this](const QString &path) { loadDocument(path, true); });
    connect(_documentLibrary, &EBDocumentLibrary::newRequested,
            this, &EBMainWindow::newDocument);
    connect(_documentLibrary, &EBDocumentLibrary::duplicateRequested,
            this, &EBMainWindow::duplicateDocument);
    connect(_documentLibrary, &EBDocumentLibrary::renameRequested,
            this, &EBMainWindow::renameDocument);
    connect(_documentLibrary, &EBDocumentLibrary::moveToTrashRequested,
            this, &EBMainWindow::moveDocumentToTrash);
    connect(_documentLibrary, &EBDocumentLibrary::restoreRequested,
            this, &EBMainWindow::restoreDocument);
    connect(_documentLibrary, &EBDocumentLibrary::deleteRequested,
            this, &EBMainWindow::deleteDocument);

    _webPlaceholder = new QWidget(_modeStack);
    _modeStack->addWidget(_webPlaceholder);
    _modeStack->addWidget(new QWidget(_modeStack));
    _modeStack->setObjectName(QStringLiteral("modeStack"));
    setCentralWidget(_modeStack);

    _commands = new EBCommands(this, _boardView);
    connect(_commands, &EBCommands::newDocumentRequested,
            this, &EBMainWindow::newDocument);
    connect(_commands, &EBCommands::fileImportRequested,
            this, &EBMainWindow::fileImportRequested);
    connect(_commands, &EBCommands::imageObjectInsertRequested,
            this, &EBMainWindow::insertImageObject);
    connect(_commands, &EBCommands::saveDocumentRequested,
            this, &EBMainWindow::saveDocument);
    connect(_commands, &EBCommands::exportPageImageRequested,
            this, &EBMainWindow::exportCurrentPageImage);
    connect(_commands, &EBCommands::exportDocumentPdfRequested,
            this, &EBMainWindow::exportDocumentPdf);
    connect(_commands, &EBCommands::exportDocumentPackageRequested,
            this, &EBMainWindow::exportDocumentPackage);
    connect(_commands, &EBCommands::quitRequested,
            this, &EBMainWindow::quitRequested);
    connect(_commands, &EBCommands::modeRequested,
            this, &EBMainWindow::modeRequested);
    connect(_commands,
            &EBCommands::pagePanelVisibilityRequested,
            _pagePanel, &EBPagePanel::setVisible);
    connect(_commands, &EBCommands::displayViewRequested,
            this, &EBMainWindow::setDisplayVisible);
    restoreLastDocument();
}

void EBMainWindow::saveDocument()
{
    saveCurrentDocument(true);
}

void EBMainWindow::newDocument()
{
    if (!prepareForImportedDocument()) {
        statusBar()->showMessage(tr("当前文档保存失败，未新建文档"), 5000);
        return;
    }
    EBDocument document;
    QString path;
    QString error;
    if (!activateImportedDocument(document, &path, &error)) {
        statusBar()->showMessage(tr("新建文档失败：%1").arg(error), 5000);
        return;
    }
    statusBar()->showMessage(tr("已新建文档"), 5000);
}

void EBMainWindow::duplicateDocument(const QString &path)
{
    if (QFileInfo(path).completeBaseName().compare(
            _document.id(), Qt::CaseInsensitive) == 0
        && !saveCurrentDocument(false)) {
        statusBar()->showMessage(tr("当前文档保存失败，未复制文档"), 5000);
        return;
    }
    QString newPath;
    QString error;
    if (!EBDocumentStorage::duplicateDocument(path, &newPath, &error)) {
        statusBar()->showMessage(tr("复制文档失败：%1").arg(error), 5000);
        return;
    }
    refreshDocumentLibrary();
    statusBar()->showMessage(tr("文档副本已创建"), 5000);
}

void EBMainWindow::exportCurrentPageImage()
{
    _boardView->commitCurrentPage();
    const QString suggested = QStringLiteral("%1-第%2页.png")
        .arg(exportBaseName(_document.title()))
        .arg(_document.currentPageIndex() + 1);
    const QString pngFilter = tr("PNG 图片 (*.png)");
    const QString jpegFilter = tr("JPEG 图片 (*.jpg *.jpeg)");
    QString selectedFilter = pngFilter;
    QString path = QFileDialog::getSaveFileName(
        this, tr("导出当前页图片"), suggested,
        pngFilter + QStringLiteral(";;") + jpegFilter, &selectedFilter);
    if (path.isEmpty())
        return;
    if (QFileInfo(path).suffix().isEmpty())
        path += selectedFilter == jpegFilter ? QStringLiteral(".jpg")
                                             : QStringLiteral(".png");

    QString savedPath;
    QString error;
    if (!EBDocumentExporter::exportPageImage(
            *_document.currentPage(), path, &savedPath, &error)) {
        statusBar()->showMessage(tr("导出图片失败：%1").arg(error), 5000);
        return;
    }
    statusBar()->showMessage(
        tr("当前页已导出：%1").arg(QDir::toNativeSeparators(savedPath)), 5000);
}

void EBMainWindow::exportDocumentPdf()
{
    _boardView->commitCurrentPage();
    const QString suggested = exportBaseName(_document.title())
                              + QStringLiteral(".pdf");
    const QString path = QFileDialog::getSaveFileName(
        this, tr("导出整份文档 PDF"), suggested, tr("PDF 文件 (*.pdf)"));
    if (path.isEmpty())
        return;

    QString savedPath;
    QString error;
    if (!EBDocumentExporter::exportDocumentPdf(
            _document, path, &savedPath, &error)) {
        statusBar()->showMessage(tr("导出 PDF 失败：%1").arg(error), 5000);
        return;
    }
    statusBar()->showMessage(
        tr("文档已导出：%1").arg(QDir::toNativeSeparators(savedPath)), 5000);
}

void EBMainWindow::exportDocumentPackage()
{
    _boardView->commitCurrentPage();
    const QString suggested = exportBaseName(_document.title())
                              + QStringLiteral(".ebz");
    const QString path = QFileDialog::getSaveFileName(
        this, tr("导出 EasyBoard 课程文档包"), suggested,
        EBDocumentPackage::fileDialogFilter());
    if (path.isEmpty())
        return;

    QString savedPath;
    QString error;
    if (!EBDocumentPackage::exportDocument(
            _document, path, &savedPath, &error)) {
        statusBar()->showMessage(tr("导出文档包失败：%1").arg(error), 5000);
        return;
    }
    statusBar()->showMessage(
        tr("文档包已导出：%1").arg(QDir::toNativeSeparators(savedPath)), 5000);
}

bool EBMainWindow::saveCurrentDocument(bool showMessage)
{
    _boardView->commitCurrentPage();
    QString path;
    QString error;
    if (EBDocumentStorage::save(_document, &path, &error)) {
        EBSettings *settings = EBSettings::settings();
        settings->setLastDocumentPath(path);
        settings->save();
        refreshDocumentLibrary();
        if (showMessage)
            statusBar()->showMessage(
                tr("文档已保存：%1").arg(QDir::toNativeSeparators(path)), 5000);
        return true;
    }
    if (showMessage)
        statusBar()->showMessage(tr("保存失败：%1").arg(error), 5000);
    return false;
}

bool EBMainWindow::openDocument(const QString &path)
{
    return loadDocument(path, true);
}

bool EBMainWindow::importImage(const QString &path)
{
    EBDocument imported;
    QString error;
    if (!EBImageImporter::importFile(path, &imported, &error)) {
        statusBar()->showMessage(tr("导入失败：%1").arg(error), 5000);
        return false;
    }

    if (!prepareForImportedDocument()) {
        statusBar()->showMessage(tr("当前文档保存失败，图片未导入"), 5000);
        return false;
    }

    QString savedPath;
    if (!activateImportedDocument(imported, &savedPath, &error)) {
        statusBar()->showMessage(tr("导入文档保存失败：%1").arg(error), 5000);
        return false;
    }
    statusBar()->showMessage(
        tr("图片已导入：%1").arg(QDir::toNativeSeparators(path)), 5000);
    return true;
}

bool EBMainWindow::importPdf(const QString &path)
{
    EBDocument imported;
    QString error;
    QEventLoop loop;
    QProgressDialog progress(tr("正在导入 PDF..."), QString(), 0, 0, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setCancelButton(nullptr);
    QThread *worker = QThread::create([&]() {
        EBPDFImporter::importFile(path, &imported, &error);
    });
    connect(worker, &QThread::finished, &loop, &QEventLoop::quit);
    worker->start();
    progress.show();
    if (worker->isRunning())
        loop.exec();
    worker->wait();
    delete worker;
    progress.close();
    if (!error.isEmpty()) {
        statusBar()->showMessage(tr("导入 PDF 失败：%1").arg(error), 5000);
        return false;
    }
    if (!prepareForImportedDocument()) {
        statusBar()->showMessage(tr("当前文档保存失败，PDF 未导入"), 5000);
        return false;
    }
    QString savedPath;
    if (!activateImportedDocument(imported, &savedPath, &error)) {
        statusBar()->showMessage(tr("导入文档保存失败：%1").arg(error), 5000);
        return false;
    }
    statusBar()->showMessage(
        tr("PDF 已导入：%1").arg(QDir::toNativeSeparators(path)), 5000);
    return true;
}

bool EBMainWindow::insertImageObject(const QString &path)
{
    EBImageItem::State state;
    QString error;
    if (!EBImageImporter::loadObject(path, &state, &error)
        || !_boardView->insertImageObject(state)) {
        statusBar()->showMessage(tr("插入图片失败：%1").arg(error), 5000);
        return false;
    }
    statusBar()->showMessage(
        tr("图片对象已插入：%1").arg(QDir::toNativeSeparators(path)), 5000);
    return true;
}

bool EBMainWindow::importDocumentPackage(const QString &path)
{
    EBDocument imported;
    QString error;
    if (!EBDocumentPackage::importDocument(path, &imported, &error)) {
        statusBar()->showMessage(tr("导入文档包失败：%1").arg(error), 5000);
        return false;
    }
    if (!prepareForImportedDocument()) {
        statusBar()->showMessage(tr("当前文档保存失败，文档包未导入"), 5000);
        return false;
    }

    QString savedPath;
    if (!activateImportedDocument(imported, &savedPath, &error)) {
        statusBar()->showMessage(tr("导入文档包保存失败：%1").arg(error), 5000);
        return false;
    }
    statusBar()->showMessage(
        tr("文档包已导入：%1").arg(QDir::toNativeSeparators(path)), 5000);
    return true;
}

bool EBMainWindow::prepareForImportedDocument()
{
    // 导入会切换到新文档；已有内容先保存，避免覆盖未保存的白板。
    _boardView->commitCurrentPage();
    const QString currentPath = EBDocumentStorage::documentFilePath(_document);
    return (!QFileInfo::exists(currentPath) && !documentHasContent(_document))
           || saveCurrentDocument(false);
}

bool EBMainWindow::activateImportedDocument(const EBDocument &document,
                                            QString *savedPath, QString *error)
{
    if (!EBDocumentStorage::save(document, savedPath, error))
        return false;
    _document = document;
    _boardView->reloadDocument();
    setWindowTitle(tr("%1 - EasyBoard").arg(_document.title()));
    EBSettings *settings = EBSettings::settings();
    settings->setLastDocumentPath(*savedPath);
    settings->save();
    refreshDocumentLibrary();
    emit modeRequested(EBApplicationController::MainMode::Board);
    return true;
}

bool EBMainWindow::loadDocument(const QString &path, bool switchToBoard)
{
    EBDocument loaded;
    QString error;
    if (!EBDocumentStorage::load(path, &loaded, &error)) {
        statusBar()->showMessage(tr("打开失败：%1").arg(error), 5000);
        return false;
    }

    // 解析成功后才替换当前文档，损坏文件不会清空正在编辑的内容。
    _boardView->commitCurrentPage();
    _document = loaded;
    _boardView->reloadDocument();
    setWindowTitle(tr("%1 - EasyBoard").arg(_document.title()));

    EBSettings *settings = EBSettings::settings();
    settings->setLastDocumentPath(QFileInfo(path).absoluteFilePath());
    settings->save();
    refreshDocumentLibrary();
    statusBar()->showMessage(
        tr("文档已打开：%1").arg(QDir::toNativeSeparators(path)), 5000);
    if (switchToBoard)
        emit modeRequested(EBApplicationController::MainMode::Board);
    return true;
}

void EBMainWindow::restoreLastDocument()
{
    EBSettings *settings = EBSettings::settings();
    const QString path = settings->lastDocumentPath();
    if (path.isEmpty())
        return;
    if (!QFileInfo::exists(path)) {
        settings->setLastDocumentPath(QString());
        settings->save();
        return;
    }
    loadDocument(path, false);
}

void EBMainWindow::refreshDocumentLibrary()
{
    _documentLibrary->refresh(_document.id());
}

void EBMainWindow::renameDocument(const QString &path, const QString &title)
{
    QString error;
    if (!EBDocumentStorage::renameDocument(path, title, &error)) {
        statusBar()->showMessage(tr("重命名失败：%1").arg(error), 5000);
        return;
    }
    if (QFileInfo(path).completeBaseName().compare(_document.id(),
                                                   Qt::CaseInsensitive) == 0) {
        _document.setTitle(title);
        setWindowTitle(tr("%1 - EasyBoard").arg(_document.title()));
    }
    refreshDocumentLibrary();
    statusBar()->showMessage(tr("文档已重命名"), 5000);
}

void EBMainWindow::moveDocumentToTrash(const QString &path)
{
    const bool current = QFileInfo(path).completeBaseName().compare(
        _document.id(), Qt::CaseInsensitive) == 0;
    QString source = path;
    if (current) {
        if (!saveCurrentDocument(false)) {
            statusBar()->showMessage(tr("当前文档保存失败，未移入回收站"), 5000);
            return;
        }
        source = EBDocumentStorage::documentFilePath(_document);
    }

    QString error;
    if (!EBDocumentStorage::moveToTrash(source, nullptr, &error)) {
        statusBar()->showMessage(tr("回收失败：%1").arg(error), 5000);
        return;
    }
    if (current) {
        const QVector<EBDocumentSummary> remaining = EBDocumentStorage::listDocuments();
        if (remaining.isEmpty() || !loadDocument(remaining.first().path, false))
            resetCurrentDocument();
    }
    refreshDocumentLibrary();
    statusBar()->showMessage(tr("文档已移入回收站"), 5000);
}

void EBMainWindow::restoreDocument(const QString &path)
{
    QString restoredPath;
    QString error;
    if (!EBDocumentStorage::restoreFromTrash(path, &restoredPath, &error)) {
        statusBar()->showMessage(tr("恢复失败：%1").arg(error), 5000);
        return;
    }
    refreshDocumentLibrary();
    statusBar()->showMessage(tr("文档已恢复"), 5000);
}

void EBMainWindow::deleteDocument(const QString &path)
{
    QString error;
    if (!EBDocumentStorage::deleteFromTrash(path, &error)) {
        statusBar()->showMessage(tr("删除失败：%1").arg(error), 5000);
        return;
    }
    refreshDocumentLibrary();
    statusBar()->showMessage(tr("文档已永久删除"), 5000);
}

void EBMainWindow::resetCurrentDocument()
{
    _document = EBDocument();
    _boardView->reloadDocument();
    setWindowTitle(tr("EasyBoard"));
    EBSettings *settings = EBSettings::settings();
    settings->setLastDocumentPath(QString());
    settings->save();
}
