#include "ebwebworkspace.h"

#include <QAction>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFrame>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMenu>
#include <QSizePolicy>
#include <QStyle>
#include <QTabWidget>
#include <QTabBar>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWebEngineHistory>
#include <QWebEngineProfile>

#include "../core/ebsettings.h"
#include "ebbarstyle.h"
#include "ebicons.h"
#include "ebwebcapture.h"
#include "ebwebdownloads.h"
#include "ebwebview.h"
#include "ebwebsession.h"
#include "ebwebhistory.h"
#include "ebwebhistorydialog.h"
#include "ebwebbookmarks.h"
#include "ebwebbookmarksdialog.h"

namespace {
QString welcomeHtml()
{
    return QStringLiteral(
        "<!doctype html><html><meta charset='utf-8'>"
        "<body style='font-family:sans-serif;color:#344854;background:#f8fafb;"
        "display:grid;place-items:center;height:90vh'>"
        "<div style='text-align:center'><h2>%1</h2><p>%2</p></div></body></html>")
        .arg(QCoreApplication::translate("EBWebWorkspace", "EasyBoard 网页").toHtmlEscaped(),
             QCoreApplication::translate("EBWebWorkspace",
                 "在上方地址栏输入网址开始浏览").toHtmlEscaped());
}
}

EBWebWorkspace::EBWebWorkspace(QWidget *parent)
    : QWidget(parent)
    , _tabs(new QTabWidget(this))
    , _profile(new QWebEngineProfile(QStringLiteral("EasyBoard"), this))
    , _address(new QLineEdit(this))
    , _history(new EBWebHistory(this))
    , _bookmarks(new EBWebBookmarks(this))
    , _downloads(new EBWebDownloads(_profile, this))
    , _backAction(nullptr)
    , _forwardAction(nullptr)
    , _reloadAction(nullptr)
    , _externalAction(nullptr)
    , _captureAction(nullptr)
    , _sessionTimer(new QTimer(this))
{
    setObjectName(QStringLiteral("webWorkspace"));
    const QDir webDirectory(QDir(EBSettings::userDataDir()).filePath(
        QStringLiteral("web")));
    QDir().mkpath(webDirectory.path());
    _profile->setPersistentStoragePath(webDirectory.filePath(
        QStringLiteral("profile")));
    _profile->setCachePath(webDirectory.filePath(QStringLiteral("cache")));
    connect(_downloads, &EBWebDownloads::statusMessage,
            this, &EBWebWorkspace::statusMessage);
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    QToolBar *bar = new QToolBar(tr("网页导航"), this);
    bar->setObjectName(QStringLiteral("webNavigationBar"));
    ebStyleBar(bar, QStringLiteral("border-bottom: 1px solid #DCE6EA;"), true);
    layout->addWidget(bar);

    _backAction = bar->addAction(style()->standardIcon(QStyle::SP_ArrowBack),
                                 tr("后退"));
    _backAction->setObjectName(QStringLiteral("webBackAction"));
    _forwardAction = bar->addAction(
        style()->standardIcon(QStyle::SP_ArrowForward), tr("前进"));
    _forwardAction->setObjectName(QStringLiteral("webForwardAction"));
    _reloadAction = bar->addAction(
        style()->standardIcon(QStyle::SP_BrowserReload), tr("刷新"));
    _reloadAction->setObjectName(QStringLiteral("webReloadAction"));
    _reloadAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
    QFrame *addressBox = new QFrame(bar);
    addressBox->setObjectName(QStringLiteral("webAddressBox"));
    addressBox->setMinimumWidth(260);
    addressBox->setFixedHeight(36);
    addressBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    addressBox->setStyleSheet(QStringLiteral(
        "QFrame#webAddressBox { background: white; border: 1px solid #C8D5DC; border-radius: 6px; }"
        "QLineEdit#webAddressEdit { background: transparent; border: none; padding: 0 8px; font-size: 16px; }"
        "QToolButton#webAddressClearButton { background: transparent; border: none; color: #425D6B; font-size: 22px; padding: 0; }"
        "QToolButton#webAddressClearButton:hover { background: #EAF4F2; border-radius: 4px; }"));
    QHBoxLayout *addressLayout = new QHBoxLayout(addressBox);
    addressLayout->setContentsMargins(0, 0, 5, 0);
    addressLayout->setSpacing(0);
    _address->setObjectName(QStringLiteral("webAddressEdit"));
    _address->setPlaceholderText(tr("输入网址"));
    _address->setFrame(false);
    _address->setMinimumHeight(30);
    addressLayout->addWidget(_address, 1);
    QToolButton *clearAddress = new QToolButton(addressBox);
    clearAddress->setObjectName(QStringLiteral("webAddressClearButton"));
    clearAddress->setText(QStringLiteral("×"));
    clearAddress->setToolTip(tr("清除地址"));
    clearAddress->setAccessibleName(tr("清除地址"));
    clearAddress->setFixedSize(20, 20);
    clearAddress->setCursor(Qt::PointingHandCursor);
    addressLayout->addWidget(clearAddress, 0, Qt::AlignVCenter);
    clearAddress->hide();
    connect(_address, &QLineEdit::textChanged, clearAddress,
            [clearAddress](const QString &text) {
        clearAddress->setVisible(!text.isEmpty());
    });
    connect(clearAddress, &QToolButton::clicked, _address, &QLineEdit::clear);
    bar->addWidget(addressBox);
    QAction *newTab = bar->addAction(
        style()->standardIcon(QStyle::SP_FileIcon), tr("新建标签页"));
    newTab->setObjectName(QStringLiteral("webNewTabAction"));
    newTab->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));
    QAction *historyAction = bar->addAction(ebToolbarIcon("history"),
                                            tr("历史"));
    historyAction->setObjectName(QStringLiteral("webHistoryAction"));
    QAction *downloadsAction = bar->addAction(ebToolbarIcon("download"),
                                              tr("下载"));
    downloadsAction->setObjectName(QStringLiteral("webDownloadsAction"));
    downloadsAction->setToolTip(tr("下载记录"));
    connect(downloadsAction, &QAction::triggered,
            _downloads, &EBWebDownloads::showManager);
    QToolButton *bookmarksButton = new QToolButton(bar);
    bookmarksButton->setObjectName(QStringLiteral("webBookmarksButton"));
    bookmarksButton->setIcon(ebToolbarIcon("bookmark"));
    bookmarksButton->setText(tr("书签"));
    bookmarksButton->setToolTip(tr("管理书签"));
    bookmarksButton->setToolButtonStyle(bar->toolButtonStyle());
    bookmarksButton->setPopupMode(QToolButton::InstantPopup);
    QMenu *bookmarksMenu = new QMenu(bookmarksButton);
    bookmarksButton->setMenu(bookmarksMenu);
    bar->addWidget(bookmarksButton);
    connect(bookmarksMenu, &QMenu::aboutToShow, this, [this, bookmarksMenu]() {
        bookmarksMenu->clear();
        QWebEngineView *view = currentView();
        const QUrl url = view ? view->url() : QUrl();
        const bool allowed = url.scheme() == QStringLiteral("http")
            || url.scheme() == QStringLiteral("https")
            || url.scheme() == QStringLiteral("file");
        QAction *add = bookmarksMenu->addAction(tr("收藏当前页"));
        add->setEnabled(allowed);
        connect(add, &QAction::triggered, this, [this, url]() {
            QWebEngineView *current = currentView();
            if (!current || current->url() != url
                || !_bookmarks->add(url, current->title()))
                emit statusMessage(tr("无法收藏当前网页"));
            else
                emit statusMessage(tr("已收藏当前网页"));
        });
        QAction *manage = bookmarksMenu->addAction(tr("管理书签"));
        connect(manage, &QAction::triggered, this, [this]() {
            EBWebBookmarksDialog *dialog = new EBWebBookmarksDialog(_bookmarks, this);
            dialog->setAttribute(Qt::WA_DeleteOnClose);
            connect(dialog, &EBWebBookmarksDialog::openRequested,
                    this, [this](const QUrl &url) { createTab(url); });
            dialog->open();
        });
        if (_bookmarks->entries().isEmpty())
            return;
        bookmarksMenu->addSeparator();
        int count = 0;
        for (const EBWebBookmarks::Entry &entry : _bookmarks->entries()) {
            if (++count > 20)
                break;
            const QString label = entry.folder.isEmpty()
                ? entry.title : entry.folder + QStringLiteral(" / ") + entry.title;
            QAction *quickOpen = bookmarksMenu->addAction(
                label.isEmpty() ? entry.url.toString() : label);
            quickOpen->setToolTip(entry.url.toString());
            connect(quickOpen, &QAction::triggered, this,
                    [this, entry]() { createTab(entry.url); });
        }
    });
    _captureAction = bar->addAction(ebToolbarIcon("image_object"), tr("截图"));
    _captureAction->setObjectName(QStringLiteral("webCaptureAction"));
    _captureAction->setToolTip(tr("截取网页区域到白板"));
    _externalAction = bar->addAction(ebToolbarIcon("web"), tr("外部打开"));
    _externalAction->setObjectName(QStringLiteral("webExternalAction"));
    _externalAction->setToolTip(tr("在外部浏览器打开"));
    _tabs->setObjectName(QStringLiteral("webTabs"));
    _tabs->setDocumentMode(true);
    _tabs->setTabsClosable(true);
    _tabs->setMovable(true);
    _sessionTimer->setSingleShot(true);
    _sessionTimer->setInterval(300);
    connect(_sessionTimer, &QTimer::timeout,
            this, &EBWebWorkspace::saveSession);
    layout->addWidget(_tabs, 1);

    connect(_backAction, &QAction::triggered, this, [this]() {
        if (currentView())
            currentView()->back();
    });
    connect(_forwardAction, &QAction::triggered, this, [this]() {
        if (currentView())
            currentView()->forward();
    });
    connect(_reloadAction, &QAction::triggered, this, [this]() {
        if (currentView())
            currentView()->reload();
    });
    connect(_address, &QLineEdit::returnPressed, this, [this]() {
        navigate(_address->text());
    });
    connect(newTab, &QAction::triggered, this, [this]() {
        createTab();
        _address->setFocus();
        _address->selectAll();
    });
    connect(historyAction, &QAction::triggered, this, [this]() {
        EBWebHistoryDialog *dialog = new EBWebHistoryDialog(_history, this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(dialog, &EBWebHistoryDialog::openRequested,
                this, [this](const QUrl &url) { createTab(url); });
        dialog->open();
    });
    connect(_captureAction, &QAction::triggered,
            this, &EBWebWorkspace::startCapture);
    connect(_externalAction, &QAction::triggered, this, [this]() {
        QWebEngineView *view = currentView();
        if (view && view->url().scheme().startsWith(QStringLiteral("http"))) {
            if (!QDesktopServices::openUrl(view->url()))
                emit statusMessage(tr("无法打开外部浏览器"));
        }
    });
    connect(_tabs, &QTabWidget::currentChanged, this, [this]() {
        refreshNavigation();
        scheduleSessionSave();
    });
    connect(_tabs->tabBar(), &QTabBar::tabMoved,
            this, &EBWebWorkspace::scheduleSessionSave);
    connect(_tabs, &QTabWidget::tabCloseRequested,
            this, &EBWebWorkspace::closeTab);
    _restoringSession = true;
    const EBWebSession session = EBWebSession::load();
    for (const QString &address : session.addresses) {
        const QUrl url(address);
        createTab(address.isEmpty() || !url.isValid() ? QUrl() : url);
    }
    if (_tabs->count() == 0)
        createTab();
    _tabs->setCurrentIndex(qBound(0, session.currentIndex, _tabs->count() - 1));
    _restoringSession = false;
    refreshNavigation();
}

EBWebWorkspace::~EBWebWorkspace()
{
    saveSession();
}

QWebEngineView *EBWebWorkspace::createTab(const QUrl &url)
{
    EBWebView *view = new EBWebView(this);
    view->setProperty("welcomePage", url.isEmpty());
    const int index = _tabs->addTab(view, tr("新标签页"));
    _tabs->setCurrentIndex(index);
    connect(view, &QWebEngineView::titleChanged, this,
            [this, view](const QString &title) {
        const int index = _tabs->indexOf(view);
        if (index >= 0)
            _tabs->setTabText(index, title.isEmpty()
                              ? tr("新标签页") : title.left(24));
    });
    connect(view, &QWebEngineView::urlChanged, this,
            [this, view](const QUrl &newUrl) {
        if (newUrl.scheme() != QStringLiteral("data")
            && newUrl != QUrl(QStringLiteral("about:blank")))
            view->setProperty("welcomePage", false);
        if (currentView() == view)
            refreshNavigation();
        scheduleSessionSave();
    });
    connect(view, &QWebEngineView::loadFinished, this,
            [this, view](bool success) {
        if (currentView() == view)
            refreshNavigation();
        if (success && !view->property("welcomePage").toBool())
            _history->record(view->url(), view->title());
        if (!success && !view->property("welcomePage").toBool())
            emit statusMessage(tr("网页加载失败：%1").arg(view->url().toString()));
    });
    if (url.isEmpty())
        view->setHtml(welcomeHtml());
    else
        view->load(url);
    refreshNavigation();
    scheduleSessionSave();
    return view;
}

void EBWebWorkspace::retranslate()
{
    QToolBar *bar = findChild<QToolBar *>(QStringLiteral("webNavigationBar"));
    bar->setWindowTitle(tr("网页导航"));
    const struct { const char *name; const char *source; } actions[] = {
        {"webBackAction", "后退"}, {"webForwardAction", "前进"},
        {"webReloadAction", "刷新"}, {"webNewTabAction", "新建标签页"},
        {"webHistoryAction", "历史"}, {"webDownloadsAction", "下载"},
        {"webCaptureAction", "截图"},
        {"webExternalAction", "外部打开"}
    };
    for (const auto &entry : actions) {
        if (QAction *action = findChild<QAction *>(QString::fromLatin1(entry.name)))
            action->setText(tr(entry.source));
    }
    _address->setPlaceholderText(tr("输入网址"));
    QToolButton *clear = findChild<QToolButton *>(QStringLiteral("webAddressClearButton"));
    clear->setToolTip(tr("清除地址"));
    clear->setAccessibleName(tr("清除地址"));
    findChild<QToolButton *>(QStringLiteral("webBookmarksButton"))->setText(tr("书签"));
    findChild<QToolButton *>(QStringLiteral("webBookmarksButton"))
        ->setToolTip(tr("管理书签"));
    findChild<QAction *>(QStringLiteral("webDownloadsAction"))
        ->setToolTip(tr("下载记录"));
    _captureAction->setToolTip(tr("截取网页区域到白板"));
    _externalAction->setToolTip(tr("在外部浏览器打开"));
    for (int index = 0; index < _tabs->count(); ++index) {
        QWebEngineView *view = qobject_cast<QWebEngineView *>(_tabs->widget(index));
        if (view && view->property("welcomePage").toBool()) {
            _tabs->setTabText(index, tr("新标签页"));
            view->setHtml(welcomeHtml());
        }
    }
}

QWebEngineView *EBWebWorkspace::currentView() const
{
    return qobject_cast<QWebEngineView *>(_tabs->currentWidget());
}

QWebEngineProfile *EBWebWorkspace::profile() const
{
    return _profile;
}

int EBWebWorkspace::tabCount() const
{
    return _tabs->count();
}

void EBWebWorkspace::navigate(const QString &address)
{
    const QString input = address.trimmed();
    if (input.isEmpty() || !currentView())
        return;
    const QUrl url = QUrl::fromUserInput(input);
    if (!url.isValid()) {
        emit statusMessage(tr("网址无效"));
        return;
    }
    currentView()->setProperty("welcomePage", false);
    currentView()->load(url);
}

void EBWebWorkspace::startCapture()
{
    if (!currentView())
        return;
    if (_capture) {
        _capture->raise();
        _capture->setFocus();
        return;
    }
    _capture = new EBWebCapture(currentView());
    connect(_capture, &EBWebCapture::captured,
            this, &EBWebWorkspace::imageCaptured);
}

void EBWebWorkspace::closeTab(int index)
{
    if (index < 0 || index >= _tabs->count())
        return;
    if (_tabs->count() == 1) {
        currentView()->setProperty("welcomePage", true);
        currentView()->setHtml(welcomeHtml());
        scheduleSessionSave();
        return;
    }
    QWidget *view = _tabs->widget(index);
    _tabs->removeTab(index);
    view->deleteLater();
    refreshNavigation();
    scheduleSessionSave();
}

void EBWebWorkspace::scheduleSessionSave()
{
    if (!_restoringSession)
        _sessionTimer->start();
}

void EBWebWorkspace::saveSession() const
{
    EBWebSession session;
    for (int index = 0; index < _tabs->count(); ++index) {
        QWebEngineView *view = qobject_cast<QWebEngineView *>(_tabs->widget(index));
        const bool welcome = view && view->property("welcomePage").toBool();
        session.addresses.append(view && !welcome ? view->url().toString()
                                                  : QString());
    }
    session.currentIndex = _tabs->currentIndex();
    session.save();
}

void EBWebWorkspace::refreshNavigation()
{
    QWebEngineView *view = currentView();
    _backAction->setEnabled(view && view->history()->canGoBack());
    _forwardAction->setEnabled(view && view->history()->canGoForward());
    _reloadAction->setEnabled(view != nullptr);
    _captureAction->setEnabled(view != nullptr);
    const bool welcome = view && view->property("welcomePage").toBool();
    _externalAction->setEnabled(view && !welcome
        && view->url().scheme().startsWith(
        QStringLiteral("http")));
    if (view && !_address->hasFocus())
        _address->setText(welcome ? QString() : view->url().toString());
}
