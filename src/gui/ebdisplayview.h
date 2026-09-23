#ifndef EBDISPLAYVIEW_H
#define EBDISPLAYVIEW_H

#include <QGraphicsView>

class QScreen;
class QTimer;
class EBBoardView;

// 独立展示窗口共用画板场景，输入只留在控制窗口。
class EBDisplayView : public QGraphicsView
{
    Q_OBJECT
public:
    EBDisplayView(EBBoardView *boardView, QWidget *parent = nullptr);
    void openOnPreferredScreen();

signals:
    void displayClosed();

protected:
    void drawForeground(QPainter *painter, const QRectF &rect) override;
    void keyPressEvent(QKeyEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    void syncView();

    EBBoardView *_boardView;
    QTimer *_refreshTimer;
};

#endif
