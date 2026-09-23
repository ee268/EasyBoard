#ifndef EBWEBVIEW_H
#define EBWEBVIEW_H

#include <QWebEngineView>

class EBWebWorkspace;

class EBWebView : public QWebEngineView
{
public:
    explicit EBWebView(EBWebWorkspace *workspace);

protected:
    QWebEngineView *createWindow(QWebEnginePage::WebWindowType type) override;

private:
    EBWebWorkspace *_workspace;
};

#endif
