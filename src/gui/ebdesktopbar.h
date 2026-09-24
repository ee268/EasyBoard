#ifndef EBDESKTOPBAR_H
#define EBDESKTOPBAR_H

#include <QToolBar>
#include <QColor>

#include "ebdesktopoverlay.h"

class QAction;

// 桌面工具栏只表达绘图、历史和退出操作。
class EBDesktopBar : public QToolBar
{
    Q_OBJECT
public:
    explicit EBDesktopBar(QWidget *parent = nullptr);
    void setHistory(bool undoAvailable, bool redoAvailable);
    void setBrushes(const QColor &penColor, qreal penWidth,
                    const QColor &markerColor, qreal markerWidth);

signals:
    void toolSelected(EBDesktopOverlay::Tool tool);
    void undoRequested();
    void redoRequested();
    void clearRequested();
    void captureRequested();
    void penColorChanged(const QColor &color);
    void markerColorChanged(const QColor &color);
    void penWidthChanged(qreal width);
    void markerWidthChanged(qreal width);
    void exitRequested();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QAction *_undoAction;
    QAction *_redoAction;
    QWidget *_dragHandle;
    QPoint _dragOrigin;
    QColor _penColor;
    QColor _markerColor;
    qreal _penWidth = 3.0;
    qreal _markerWidth = 18.0;
};

#endif
