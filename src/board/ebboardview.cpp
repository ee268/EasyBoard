#include "ebboardview.h"

#include <QEvent>
#include <QGraphicsScene>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QGraphicsPathItem>
#include <QtMath>

#include "../global/ebtheme.h"

namespace {
constexpr qreal kPageMargin = 80.0;
constexpr qreal kPageWidth = 1200.0;
constexpr qreal kPageHeight = 900.0;
constexpr qreal kPenWidth = 3.0;
constexpr qreal kMarkerWidth = 18.0;
constexpr qreal kEraserRadius = 12.0;
constexpr qreal kPointerRadius = 7.0;

// 当前三种绘图工具都产生由直线段组成的 QPainterPath；按圆形橡皮裁开这些线段。
bool splitStrokeAt(const QPainterPath &path, const QPointF &center,
                   qreal radius, QVector<QPainterPath> &remaining)
{
    QPainterPath fragment;
    QPointF lastPoint;
    bool hasFragment = false;
    bool removedPart = false;
    const qreal radiusSquared = radius * radius;
    const auto flush = [&]() {
        if (hasFragment)
            remaining.append(fragment);
        fragment = QPainterPath();
        hasFragment = false;
    };

    for (int index = 1; index < path.elementCount(); ++index) {
        const QPainterPath::Element previous = path.elementAt(index - 1);
        const QPainterPath::Element current = path.elementAt(index);
        if (current.isMoveTo()) {
            flush();
            continue;
        }
        const QPointF start(previous.x, previous.y);
        const QPointF end(current.x, current.y);
        const QPointF direction = end - start;
        const QPointF offset = start - center;
        const qreal a = QPointF::dotProduct(direction, direction);
        QVector<qreal> cuts{0.0, 1.0};
        if (a > 1e-12) {
            // 求线段与橡皮圆的交点参数 t，再判定各区间保留还是擦除。
            const qreal b = 2.0 * QPointF::dotProduct(offset, direction);
            const qreal c = QPointF::dotProduct(offset, offset) - radiusSquared;
            const qreal discriminant = b * b - 4.0 * a * c;
            if (discriminant > 0.0) {
                const qreal root = qSqrt(discriminant);
                for (qreal t : {(-b - root) / (2.0 * a),
                                (-b + root) / (2.0 * a)}) {
                    if (t > 1e-7 && t < 1.0 - 1e-7)
                        cuts.append(t);
                }
                std::sort(cuts.begin(), cuts.end());
            }
        }

        for (int part = 1; part < cuts.size(); ++part) {
            const qreal from = cuts.at(part - 1);
            const qreal to = cuts.at(part);
            if (to - from <= 1e-7)
                continue;
            const QPointF middle = start + direction * ((from + to) / 2.0);
            const QPointF distance = middle - center;
            if (QPointF::dotProduct(distance, distance) < radiusSquared) {
                // 圆内区间被删除；在此结束当前片段，下一段另起一条路径。
                removedPart = true;
                flush();
                continue;
            }

            const QPointF first = start + direction * from;
            const QPointF second = start + direction * to;
            if (!hasFragment || QLineF(lastPoint, first).length() > 1e-5) {
                flush();
                fragment.moveTo(first);
                hasFragment = true;
            }
            fragment.lineTo(second);
            lastPoint = second;
        }
    }
    flush();
    return removedPart;
}
}

EBBoardView::EBBoardView(QWidget *parent)
    : QGraphicsView(parent)
    , _scene(new QGraphicsScene(this))
    , _pageRect(kPageMargin, kPageMargin, kPageWidth, kPageHeight)
    , _activeStroke(nullptr)
    , _drawingTool(DrawingTool::Pen)
    , _activeTool(DrawingTool::Pen)
    , _erasing(false)
    , _pointing(false)
    , _pointerItem(nullptr)
{
    setObjectName(QStringLiteral("boardView"));
    setScene(_scene);

    //宽高分别乘以边距，使addRect添加的矩形居中于sceneRect
    _scene->setSceneRect(0.0, 0.0,
                         kPageWidth + 2.0 * kPageMargin,
                         kPageHeight + 2.0 * kPageMargin);
    _scene->addRect(_pageRect,
                    QPen(ebThemeColor(EBThemeColor::BoardBorder)),
                    QBrush(ebThemeColor(EBThemeColor::Board)));

    // 红色圆点只用于讲解时指示位置，初始隐藏且不计入画笔笔迹。
    _pointerItem = _scene->addEllipse(-kPointerRadius, -kPointerRadius,
                                      2.0 * kPointerRadius, 2.0 * kPointerRadius,
                                      QPen(ebThemeColor(EBThemeColor::BoardPointerBorder), 2.0),
                                      QBrush(ebThemeColor(EBThemeColor::BoardPointerFill)));
    _pointerItem->setZValue(2.0);
    _pointerItem->hide();

    setBackgroundBrush(ebThemeColor(EBThemeColor::BoardBackground));
    setRenderHint(QPainter::Antialiasing);
    setFrameShape(QFrame::NoFrame);

    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    //在没有按下鼠标按钮时也能收到鼠标移动事件
    setMouseTracking(true);
    //对实际显示白板的视口控件开启同一功能
    viewport()->setMouseTracking(true);

    fitPage();
}

void EBBoardView::setDrawingTool(DrawingTool tool)
{
    _drawingTool = tool;
}

EBBoardView::DrawingTool EBBoardView::drawingTool() const
{
    return _drawingTool;
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

    _activeTool = _drawingTool;

    if (_activeTool == DrawingTool::Eraser) {
        _erasing = true;
        _lastEraserPosition = pagePosition;
        eraseAt(pagePosition);
        event->accept();
        return;
    }
    if (_activeTool == DrawingTool::Pointer) {
        _pointing = true;
        movePointerTo(pagePosition);
        event->accept();
        return;
    }

    _strokeStart = pagePosition;

    QPainterPath path(pagePosition);
    path.lineTo(pagePosition + QPointF(0.01, 0.0));
    const bool marker = _activeTool == DrawingTool::Marker;
    const QColor penColor = marker ?
                          ebThemeColor(EBThemeColor::BoardMarker) :
                          ebThemeColor(EBThemeColor::BoardPen);
    QPen pen(penColor, marker ? kMarkerWidth : kPenWidth,
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

    if (_erasing && (event->buttons() & Qt::LeftButton)) {
        const QPointF current = boundedPagePosition(event->pos());
        eraseAlong(_lastEraserPosition, current);
        event->accept();
        _lastEraserPosition = current;
        return;
    }
    if (_pointing && (event->buttons() & Qt::LeftButton)) {
        if (_pointerItem->isVisible())
            movePointerTo(scenePosition - _pageRect.topLeft());
        event->accept();
    }

    if (_activeStroke && (event->buttons() & Qt::LeftButton)) {
        updateActiveStroke(boundedPagePosition(event->pos()));
        event->accept();
        return;
    }

    QGraphicsView::mouseMoveEvent(event);
}

void EBBoardView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && (_erasing || _pointing)) {
        if (_erasing)
            eraseAlong(_lastEraserPosition,  boundedPagePosition(event->pos()));
        _erasing = false;
        _pointing = false;
        _pointerItem->hide();
        event->accept();
        return;
    }

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
    _pointerItem->hide();
    emit pagePositionChanged(QPointF(), false);
    QGraphicsView::leaveEvent(event);
}

void EBBoardView::hideEvent(QHideEvent *event)
{
    _erasing = false;
    _pointing = false;
    _pointerItem->hide();
    QGraphicsView::hideEvent(event);
}

void EBBoardView::fitPage()
{
    //让scene始终居中于窗口
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

void EBBoardView::updateActiveStroke(const QPointF &pagePosition)
{
    if (_activeTool == DrawingTool::Line) {
        // 每次从固定起点重建路径，所以拖动轨迹不会变成折线。
        QPainterPath line(_strokeStart);
        line.lineTo(pagePosition);
        _activeStroke->setPath(line);
    } else {
        // 画笔和荧光笔都保留鼠标经过的采样点，形成自由笔迹。
        QPainterPath path = _activeStroke->path();
        path.lineTo(pagePosition);
        _activeStroke->setPath(path);
    }
}

void EBBoardView::eraseAt(const QPointF &pagePosition)
{
    const QPointF scenePosition = pagePosition + _pageRect.topLeft();
    const QRectF touchArea(scenePosition.x() - kEraserRadius,
                           scenePosition.y() - kEraserRadius,
                           2.0 * kEraserRadius, 2.0 * kEraserRadius);
    // 只检查路径图元；页面背景和临时指示点绝不被橡皮裁切。
    for (QGraphicsItem *item : _scene->items(touchArea, Qt::IntersectsItemShape)) {
        auto *stroke = qgraphicsitem_cast<QGraphicsPathItem *>(item);
        if (!stroke)
            continue;

        QVector<QPainterPath> remaining;
        // 笔触有宽度：多裁去半个线宽，避免新片段的圆头重新伸入擦除区。
        const qreal radius = kEraserRadius + stroke->pen().widthF() / 2.0;
        if (!splitStrokeAt(stroke->path(), stroke->mapFromScene(scenePosition),
                           radius, remaining))
            continue;

        const QPen pen = stroke->pen();
        const QPointF position = stroke->pos();
        const qreal zValue = stroke->zValue();
        delete stroke;
        // 只把圆外的片段放回场景，保留原来的笔色、宽度和页面位置。
        for (const QPainterPath &part : remaining) {
            QGraphicsPathItem *kept = _scene->addPath(part, pen);
            kept->setPos(position);
            kept->setZValue(zValue);
        }
    }
}

void EBBoardView::eraseAlong(const QPointF &from, const QPointF &to)
{
    const int steps = qMax(1, qCeil(QLineF(from, to).length() / kEraserRadius));

    for (int index = 1; index <= steps; ++index) {
        eraseAt(from + (to - from) * (qreal(index) / steps));
    }
}

void EBBoardView::movePointerTo(const QPointF &pagePosition)
{
    _pointerItem->setPos(pagePosition + _pageRect.topLeft());
    _pointerItem->show();
}
