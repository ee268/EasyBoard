#include "ebdisplayview.h"

#include <QCloseEvent>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QPainter>
#include <QScreen>
#include <QTimer>
#include <QWindow>

#include "../board/ebboardview.h"

EBDisplayView::EBDisplayView(EBBoardView *boardView, QWidget *parent)
    : QGraphicsView(parent)
    , _boardView(boardView)
    , _refreshTimer(new QTimer(this))
{
    setWindowFlags(Qt::Window);
    setWindowTitle(tr("EasyBoard 展示视图"));
    setObjectName(QStringLiteral("displayView"));
    setScene(_boardView->scene());
    setInteractive(false);
    setDragMode(QGraphicsView::NoDrag);
    setFrameShape(QFrame::NoFrame);
    setRenderHint(QPainter::Antialiasing);
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setBackgroundBrush(QColor(30, 38, 45));
    _refreshTimer->setInterval(33);
    connect(_refreshTimer, &QTimer::timeout, this, &EBDisplayView::syncView);
}

void EBDisplayView::retranslate()
{
    setWindowTitle(tr("EasyBoard 展示视图"));
}

void EBDisplayView::openOnPreferredScreen()
{
    const QList<QScreen *> screens = QGuiApplication::screens();
    if (screens.size() > 1) {
        QScreen *controlScreen = _boardView->window()->windowHandle()
            ? _boardView->window()->windowHandle()->screen() : screens.first();
        QScreen *displayScreen = screens.first();
        for (QScreen *screen : screens) {
            if (screen != controlScreen) {
                displayScreen = screen;
                break;
            }
        }
        setGeometry(displayScreen->geometry());
        showFullScreen();
    } else {
        resize(900, 600);
        if (!screens.isEmpty()) {
            const QRect available = screens.first()->availableGeometry();
            move(available.center() - rect().center());
        }
        showNormal();
    }
    _refreshTimer->start();
    syncView();
    raise();
}

void EBDisplayView::drawForeground(QPainter *painter, const QRectF &rect)
{
    QGraphicsView::drawForeground(painter, rect);
    _boardView->paintTeachingTools(painter);
}

void EBDisplayView::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        close();
        event->accept();
        return;
    }
    QGraphicsView::keyPressEvent(event);
}

void EBDisplayView::closeEvent(QCloseEvent *event)
{
    _refreshTimer->stop();
    QGraphicsView::closeEvent(event);
    emit displayClosed();
}

void EBDisplayView::syncView()
{
    if (!isVisible() || !_boardView->scene())
        return;
    resetTransform();
    fitInView(_boardView->scene()->sceneRect(), Qt::KeepAspectRatio);
    scale(_boardView->zoomFactor(), _boardView->zoomFactor());
    centerOn(_boardView->mapToScene(_boardView->viewport()->rect().center()));
    viewport()->update();
}
