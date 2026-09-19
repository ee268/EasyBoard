#include "ebmainwindow.h"

#include "../board/ebboardview.h"
#include "../core/ebsettings.h"
#include "../persistence/ebdocumentstorage.h"
#include "ebmainwindowactions.h"
#include "ebpagenavigator.h"

#include <QDebug>
#include <QHBoxLayout>
#include <QLabel>
#include <QDir>
#include <QFileInfo>
#include <QStackedWidget>
#include <QStatusBar>

EBMainWindow::EBMainWindow(QWidget *parent)
    : QMainWindow{parent}
    , _modeStack(new QStackedWidget(this))
    , _boardWorkspace(new QWidget(_modeStack))
    , _boardView(new EBBoardView(&_document, _boardWorkspace))
    , _pageNavigator(new EBPageNavigator(&_document, _boardWorkspace))
    , _actions(nullptr)
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
    boardLayout->addWidget(_pageNavigator);
    boardLayout->addWidget(_boardView, 1);
    _modeStack->addWidget(_boardWorkspace);
    connect(_pageNavigator, &EBPageNavigator::pageSelected,
            _boardView, &EBBoardView::setCurrentPageIndex);
    connect(_pageNavigator, &EBPageNavigator::addPageRequested,
            _boardView, &EBBoardView::addPage);
    connect(_pageNavigator, &EBPageNavigator::duplicatePageRequested,
            _boardView, &EBBoardView::duplicateCurrentPage);
    connect(_pageNavigator, &EBPageNavigator::removePageRequested,
            _boardView, &EBBoardView::removeCurrentPage);
    connect(_pageNavigator, &EBPageNavigator::movePageRequested,
            _boardView, &EBBoardView::moveCurrentPage);
    connect(_boardView, &EBBoardView::pageListChanged,
            _pageNavigator, &EBPageNavigator::refreshPages);
    connect(_boardView, &EBBoardView::currentPageChanged,
            _pageNavigator, &EBPageNavigator::setCurrentPageIndex);
    connect(_boardView, &EBBoardView::pageContentChanged,
            _pageNavigator, &EBPageNavigator::refreshPage);
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

    for (const QString &name : {tr("文档"), tr("网页"), tr("桌面")}) {
        QLabel *placeholder = new QLabel(
            tr("%1工作区\n\n阶段 7：仅演示模式切换，实际功能尚未接入").arg(name),
            _modeStack);
        placeholder->setAlignment(Qt::AlignCenter);
        _modeStack->addWidget(placeholder);
    }
    _modeStack->setObjectName(QStringLiteral("modeStack"));
    setCentralWidget(_modeStack);

    _actions = new EBMainWindowActions(this, _boardView);
    connect(_actions, &EBMainWindowActions::fileImportRequested,
            this, &EBMainWindow::fileImportRequested);
    connect(_actions, &EBMainWindowActions::saveDocumentRequested,
            this, &EBMainWindow::saveDocument);
    connect(_actions, &EBMainWindowActions::quitRequested,
            this, &EBMainWindow::quitRequested);
    connect(_actions, &EBMainWindowActions::modeRequested,
            this, &EBMainWindow::modeRequested);
    restoreLastDocument();
}

void EBMainWindow::saveDocument()
{
    _boardView->commitCurrentPage();
    QString path;
    QString error;
    if (EBDocumentStorage::save(_document, &path, &error)) {
        EBSettings *settings = EBSettings::settings();
        settings->setLastDocumentPath(path);
        settings->save();
        statusBar()->showMessage(
            tr("文档已保存：%1").arg(QDir::toNativeSeparators(path)), 5000);
    } else {
        statusBar()->showMessage(tr("保存失败：%1").arg(error), 5000);
    }
}

bool EBMainWindow::openDocument(const QString &path)
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
    statusBar()->showMessage(
        tr("文档已打开：%1").arg(QDir::toNativeSeparators(path)), 5000);
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
    openDocument(path);
}

void EBMainWindow::showMode(EBApplicationController::MainMode mode)
{
    const int index = static_cast<int>(mode);
    if (index < 0 || index >= _modeStack->count())
        return;

    // 工作区顺序与模式枚举一致；动作对象同步勾选和可用状态。
    _modeStack->setCurrentIndex(index);
    _actions->setMode(mode);
}
