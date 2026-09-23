#include "ebwebview.h"

#include "ebwebworkspace.h"

#include <QWebEnginePage>

EBWebView::EBWebView(EBWebWorkspace *workspace)
    : QWebEngineView(workspace)
    , _workspace(workspace)
{
    setPage(new QWebEnginePage(_workspace->profile(), this));
}

QWebEngineView *EBWebView::createWindow(QWebEnginePage::WebWindowType type)
{
    Q_UNUSED(type)
    return _workspace->createTab();
}
