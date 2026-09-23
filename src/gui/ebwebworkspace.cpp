#include "ebwebworkspace.h"

#include <QAction>
#include <QDesktopServices>
#include <QDir>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QStyle>
#include <QTabWidget>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWebEngineHistory>
#include <QWebEngineProfile>

#include "../core/ebsettings.h"
#include "ebbarstyle.h"
#include "ebicons.h"
#include "ebwebcapture.h"
#include "ebwebview.h"

namespace {
const char *welcomeHtml =
    "<!doctype html><html lang='zh'><meta charset='utf-8'>"
    "<body style='font-family:sans-serif;color:#344854;background:#f8fafb;"
    "display:grid;place-items:center;height:90vh'>"
    "<div style='text-align:center'><h2>EasyBoard 网页</h2>"
    "<p>在上方地址栏输入网址开始浏览</p></div></body></html>";
}

EBWebWorkspace::EBWebWorkspace(QWidget *parent)
    : QWidget(parent)
    , _tabs(new QTabWidget(this))
    , _profile(new QWebEngineProfile(QStringLiteral("EasyBoard"), this))
    , _address(new QLineEdit(this))
    , _backAction(nullptr)
    , _forwardAction(nullptr)
    , _reloadAction(nullptr)
    , _externalAction(nullptr)
    , _captureAction(nullptr)
{
    setObjectName(QStringLiteral("webWorkspace"));
    const QDir webDirectory(QDir(EBSettings::userDataDir()).filePath(
        QStringLiteral("web")));
    QDir().mkpath(webDirectory.path());
    _profile->setPersistentStoragePath(webDirectory.filePath(
        QStringLiteral("profile")));
    _profile->setCachePath(webDirectory.filePath(QStringLiteral("cache")));
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    QToolBar *bar = new QToolBar(tr("网页导航"), this);
    bar->setObjectName(QStringLiteral("webNavigationBar"));
    ebStyleBar(bar, QStringLiteral("border-bottom: 1px solid #DCE6EA;"));
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
    _address->setObjectName(QStringLiteral("webAddressEdit"));
    _address->setPlaceholderText(tr("输入网址"));
    _address->setClearButtonEnabled(true);
    _address->setMinimumWidth(220);
    bar->addWidget(_address);
    QAction *newTab = bar->addAction(
        style()->standardIcon(QStyle::SP_FileIcon), tr("新建标签页"));
    newTab->setObjectName(QStringLiteral("webNewTabAction"));
    newTab->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));
    _captureAction = bar->addAction(ebToolbarIcon("image_object"),
                                    tr("截取网页区域到白板"));
    _captureAction->setObjectName(QStringLiteral("webCaptureAction"));
    _externalAction = bar->addAction(ebToolbarIcon("web"),
                                     tr("在外部浏览器打开"));
    _externalAction->setObjectName(QStringLiteral("webExternalAction"));
    _tabs->setObjectName(QStringLiteral("webTabs"));
    _tabs->setDocumentMode(true);
    _tabs->setTabsClosable(true);
    _tabs->setMovable(true);
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
    connect(_captureAction, &QAction::triggered,
            this, &EBWebWorkspace::startCapture);
    connect(_externalAction, &QAction::triggered, this, [this]() {
        QWebEngineView *view = currentView();
        if (view && view->url().scheme().startsWith(QStringLiteral("http"))) {
            if (!QDesktopServices::openUrl(view->url()))
                emit statusMessage(tr("无法打开外部浏览器"));
        }
    });
    connect(_tabs, &QTabWidget::currentChanged,
            this, &EBWebWorkspace::refreshNavigation);
    connect(_tabs, &QTabWidget::tabCloseRequested,
            this, &EBWebWorkspace::closeTab);
    createTab();
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
    });
    connect(view, &QWebEngineView::loadFinished, this,
            [this, view](bool success) {
        if (currentView() == view)
            refreshNavigation();
        if (!success && !view->property("welcomePage").toBool())
            emit statusMessage(tr("网页加载失败：%1").arg(view->url().toString()));
    });
    if (url.isEmpty())
        view->setHtml(QString::fromUtf8(welcomeHtml));
    else
        view->load(url);
    refreshNavigation();
    return view;
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
        currentView()->setHtml(QString::fromUtf8(welcomeHtml));
        return;
    }
    QWidget *view = _tabs->widget(index);
    _tabs->removeTab(index);
    view->deleteLater();
    refreshNavigation();
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
