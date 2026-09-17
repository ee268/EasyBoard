#include "ebboardview.h"

#include <QEvent>
#include <QGraphicsScene>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QGraphicsPathItem>

#include "../global/ebtheme.h"

namespace {
constexpr qreal kPageMargin = 80.0;
constexpr qreal kPageWidth = 1200.0;
constexpr qreal kPageHeight = 900.0;
constexpr qreal kPenWidth = 3.0;
}

EBBoardView::EBBoardView(QWidget *parent)
    : QGraphicsView(parent)
    , _scene(new QGraphicsScene(this))
    , _pageRect(kPageMargin, kPageMargin, kPageWidth, kPageHeight)
    , _activeStroke(nullptr)
{
    setObjectName(QStringLiteral("boardView"));
    setScene(_scene);

    _scene->setSceneRect(0.0, 0.0,
                         kPageWidth + 2.0 * kPageMargin,
                         kPageHeight + 2.0 * kPageMargin);
    _scene->addRect(_pageRect,
                    QPen(ebThemeColor(EBThemeColor::BoardBorder)),
                    QBrush(ebThemeColor(EBThemeColor::Board)));

    setBackgroundBrush(ebThemeColor(EBThemeColor::BoardBackground));
    setRenderHint(QPainter::Antialiasing);
    setFrameShape(QFrame::NoFrame);

    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setMouseTracking(true);
    viewport()->setMouseTracking(true);
    fitPage();
}

QPointF EBBoardView::toPagePosition(const QPointF &viewportPos) const
{
    return mapToScene(viewportPos.toPoint()) - _pageRect.topLeft();
}

QRectF EBBoardView::pageRect() const
{
    return _pageRect;
}

void EBBoardView::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
    fitPage();
}

void EBBoardView::mousePressEvent(QMouseEvent *event)
{
    const QPointF pagePosition = toPagePosition(event->pos());
    const QRectF pageLocalRect(QPointF(), _pageRect.size());
    if (event->button() != Qt::LeftButton || !pageLocalRect.contains(pagePosition)) {
        QGraphicsView::mousePressEvent(event);
        return;
    }

    QPainterPath path(pagePosition);
    path.lineTo(pagePosition + QPointF(0.01, 0.0));
    QPen pen(ebThemeColor(EBThemeColor::BoardPen), kPenWidth,
             Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    _activeStroke = _scene->addPath(path, pen);
    _activeStroke->setPos(_pageRect.topLeft());
    _activeStroke->setZValue(1.0);
    event->accept();
}

void EBBoardView::mouseMoveEvent(QMouseEvent *event)
{
    const QPointF scenePosition = mapToScene(event->pos());
    emit pagePositionChanged(scenePosition - _pageRect.topLeft(),
                             _pageRect.contains(scenePosition));

    if (_activeStroke && (event->buttons() & Qt::LeftButton)) {
        QPainterPath path = _activeStroke->path();
        path.lineTo(boundedPagePosition(event->pos()));
        _activeStroke->setPath(path);
        event->accept();
        return;
    }

    QGraphicsView::mouseMoveEvent(event);
}

void EBBoardView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && _activeStroke) {
        QPainterPath path = _activeStroke->path();
        path.lineTo(boundedPagePosition(event->pos()));
        _activeStroke->setPath(path);
        _activeStroke = nullptr;
        event->accept();
        return;
    }
}

void EBBoardView::leaveEvent(QEvent *event)
{
    emit pagePositionChanged(QPointF(), false);
    QGraphicsView::leaveEvent(event);
}

void EBBoardView::fitPage()
{
    fitInView(_scene->sceneRect(), Qt::KeepAspectRatio);
}

QPointF EBBoardView::boundedPagePosition(const QPoint &viewportPosition) const
{
    //第一步，toPagePosition() 将鼠标在视图中的位置换算为以页面左上角为 (0, 0) 的坐标。
    //第二步，qBound(最小值, 当前值, 最大值) 把横坐标限制在 0～1200、纵坐标限制在 0～900。
    const QPointF position = toPagePosition(viewportPosition);

    return QPointF(qBound(0.0, position.x(), _pageRect.width()),
                   qBound(0.0, position.y(), _pageRect.height()));
}
