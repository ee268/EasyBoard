#ifndef EBDESKTOPBAR_H
#define EBDESKTOPBAR_H

#include <QToolBar>

#include "ebdesktopoverlay.h"

class QAction;

// 桌面工具栏只表达绘图、历史和退出操作。
class EBDesktopBar : public QToolBar
{
    Q_OBJECT
public:
    explicit EBDesktopBar(QWidget *parent = nullptr);
    void setHistory(bool undoAvailable, bool redoAvailable);

signals:
    void toolSelected(EBDesktopOverlay::Tool tool);
    void undoRequested();
    void redoRequested();
    void exitRequested();

private:
    QAction *_undoAction;
    QAction *_redoAction;
};

#endif
