#include "ebboardview.h"

#include <QHideEvent>
#include <QLineF>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QShowEvent>
#include <QUndoCommand>
#include <QUndoStack>
#include <QWheelEvent>
#include <QtMath>

#include "../global/ebtheme.h"
#include "../domain/ebdocument.h"

namespace {
constexpr qreal kPenWidth = 3.0;
constexpr qreal kMarkerWidth = 18.0;
constexpr qreal kEraserRadius = 12.0;
constexpr int kMaxHistoryEntries = 50;
constexpr qreal kZoomStep = 1.25;
constexpr qreal kMinZoomFactor = 0.5;
constexpr qreal kMaxZoomFactor = 4.0;
}

// 一次绘制或擦除是一项撤销命令；场景负责真正恢复笔迹图元。
class EBBoardView::StrokeEditCommand : public QUndoCommand
{
public:
    StrokeEditCommand(EBBoardView *view, const Snapshot &before,
                      const Snapshot &after, const QString &description)
        : QUndoCommand(description)
        , _view(view)
        , _before(before)
        , _after(after)
        , _firstRedo(true)
    {
    }

    void undo() override
    {
        _view->_scene->restoreStrokes(_before);
    }

    void redo() override
    {
        // 入栈时 Qt 会立即调用 redo；实时绘制已把场景置于操作后状态。
        if (_firstRedo) {
            _firstRedo = false;
            return;
        }
        _view->_scene->restoreStrokes(_after);
    }

private:
    EBBoardView *_view;
    Snapshot _before;
    Snapshot _after;
    bool _firstRedo;
};

EBBoardView::EBBoardView(EBDocument *document, QWidget *parent)
    : QGraphicsView(parent)
    , _document(document)
    , _scene(new EBBoardScene(this))
    , _activeStroke(nullptr)
    , _drawingTool(DrawingTool::Pen)
    , _activeTool(DrawingTool::Pen)
    , _erasing(false)
    , _pointing(false)
    , _undoStack(new QUndoStack(this))
    , _editActive(false)
    , _editChanged(false)
    , _zoomFactor(1.0)
    , _viewCenter(_scene->sceneRect().center())
    , _viewStateReady(false)
{
    Q_ASSERT(_document);
    _scene->showPage(*_document->currentPage());
    setObjectName(QStringLiteral("boardView"));
    _undoStack->setObjectName(QStringLiteral("boardUndoStack"));
    _undoStack->setUndoLimit(kMaxHistoryEntries);
    setScene(_scene);
    setBackgroundBrush(ebThemeColor(EBThemeColor::BoardBackground));
    setRenderHint(QPainter::Antialiasing);
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setMouseTracking(true);
    viewport()->setMouseTracking(true);
    fitPage();
}

void EBBoardView::setDrawingTool(DrawingTool tool)
{
    // 切换只影响下一次操作，进行中的笔迹继续使用按下时的工具。
    _drawingTool = tool;
    // 平移只改变视图，不建立笔迹或撤销历史。
    setDragMode(tool == DrawingTool::Pan ? QGraphicsView::ScrollHandDrag
                                         : QGraphicsView::NoDrag);
}

EBBoardView::DrawingTool EBBoardView::drawingTool() const
{
    return _drawingTool;
}

bool EBBoardView::canUndo() const
{
    return !_editActive && _undoStack->canUndo();
}

bool EBBoardView::canRedo() const
{
    return !_editActive && _undoStack->canRedo();
}

void EBBoardView::undo()
{
    if (!canUndo())
        return;
    _undoStack->undo();
    syncCurrentPageStrokes();
    emit historyAvailabilityChanged(canUndo(), canRedo());
}

void EBBoardView::redo()
{
    if (!canRedo())
        return;
    _undoStack->redo();
    syncCurrentPageStrokes();
    emit historyAvailabilityChanged(canUndo(), canRedo());
}

int EBBoardView::addPage()
{
    finishPageInteraction();
    const int index = _document->addPage();
    emit pageListChanged();
    setCurrentPageIndex(index);
    return index;
}

int EBBoardView::duplicateCurrentPage()
{
    finishPageInteraction();
    const int index = _document->duplicatePage(_document->currentPageIndex());
    if (index < 0)
        return -1;
    emit pageListChanged();
    setCurrentPageIndex(index);
    return index;
}

bool EBBoardView::removeCurrentPage()
{
    if (_document->pageCount() <= 1)
        return false;
    finishPageInteraction();
    if (!_document->removePage(_document->currentPageIndex()))
        return false;
    showCurrentPage();
    emit pageListChanged();
    emit currentPageChanged(_document->currentPageIndex());
    return true;
}

bool EBBoardView::moveCurrentPage(int offset)
{
    const int from = _document->currentPageIndex();
    const int to = from + offset;
    if (to < 0 || to >= _document->pageCount())
        return false;
    finishPageInteraction();
    if (!_document->movePage(from, to))
        return false;
    emit pageListChanged();
    emit currentPageChanged(_document->currentPageIndex());
    return true;
}

bool EBBoardView::setCurrentPageIndex(int index)
{
    if (!_document->pageAt(index))
        return false;
    if (index == _document->currentPageIndex())
        return true;

    // 先结束旧页上的拖动，再载入新页，撤销命令不能跨页恢复笔迹。
    finishPageInteraction();
    _document->setCurrentPageIndex(index);
    showCurrentPage();
    emit currentPageChanged(index);
    return true;
}

void EBBoardView::commitCurrentPage()
{
    // 保存前结束尚未释放鼠标的操作，确保模型包含场景中的最新笔迹。
    finishPageInteraction();
}

void EBBoardView::reloadDocument()
{
    // 文档对象已由主窗口整体替换，丢弃旧文档的交互与撤销状态。
    _activeStroke = nullptr;
    _erasing = false;
    _pointing = false;
    _editActive = false;
    _editChanged = false;
    _editBefore.clear();
    _scene->hidePointer();
    showCurrentPage();
    emit pageListChanged();
    emit currentPageChanged(_document->currentPageIndex());
}

void EBBoardView::zoomIn()
{
    zoomBy(kZoomStep);
}

void EBBoardView::zoomOut()
{
    zoomBy(1.0 / kZoomStep);
}

qreal EBBoardView::zoomFactor() const
{
    return _zoomFactor;
}

void EBBoardView::setPageColor(PageColor color)
{
    if (pageColor() == color)
        return;
    _document->currentPage()->setColor(color);
    _scene->setPageColor(color);
    emit pageContentChanged(_document->currentPageIndex());
}

EBBoardView::PageColor EBBoardView::pageColor() const
{
    return _scene->pageColor();
}

void EBBoardView::setPagePattern(PagePattern pattern)
{
    if (pagePattern() == pattern)
        return;
    _document->currentPage()->setPattern(pattern);
    _scene->setPagePattern(pattern);
    emit pageContentChanged(_document->currentPageIndex());
}

EBBoardView::PagePattern EBBoardView::pagePattern() const
{
    return _scene->pagePattern();
}

void EBBoardView::setPageSize(PageSize size)
{
    if (pageSize() == size)
        return;
    finishPageInteraction();
    _document->currentPage()->setSize(size);
    _scene->setPageSize(size);
    fitPage();
    emit pageContentChanged(_document->currentPageIndex());
}

EBBoardView::PageSize EBBoardView::pageSize() const
{
    return _scene->pageSize();
}

QRectF EBBoardView::pageRect() const
{
    return _scene->pageRect();
}

QPointF EBBoardView::toPagePosition(const QPointF &viewportPosition) const
{
    // 视口坐标先映射到场景，再减去页面在场景中的左上角。
    return mapToScene(viewportPosition.toPoint()) - pageRect().topLeft();
}

void EBBoardView::resizeEvent(QResizeEvent *event)
{
    // 直接使用上次保存的场景中心；此时视口尺寸可能已变化。
    QGraphicsView::resizeEvent(event);
    applyViewState();
}

void EBBoardView::showEvent(QShowEvent *event)
{
    QGraphicsView::showEvent(event);
    // 从其他工作区返回时，按保存的缩放与中心恢复视图。
    applyViewState();
}

void EBBoardView::wheelEvent(QWheelEvent *event)
{
    if (!(event->modifiers() & Qt::ControlModifier)) {
        QGraphicsView::wheelEvent(event);
        _viewCenter = mapToScene(viewport()->rect().center());
        return;
    }

    const QPointF before = mapToScene(event->pos());
    zoomBy(event->angleDelta().y() >= 0 ? kZoomStep : 1.0 / kZoomStep);
    // Ctrl + 滚轮时保持鼠标所指的场景位置尽量不动。
    _viewCenter = mapToScene(viewport()->rect().center())
                  + before - mapToScene(event->pos());
    centerOn(_viewCenter);
    event->accept();
}

void EBBoardView::mousePressEvent(QMouseEvent *event)
{
    if (_drawingTool == DrawingTool::Pan) {
        QGraphicsView::mousePressEvent(event);
        return;
    }
    const QPointF pagePosition = toPagePosition(event->pos());
    if (event->button() != Qt::LeftButton
        || !QRectF(QPointF(), pageRect().size()).contains(pagePosition)) {
        QGraphicsView::mousePressEvent(event);
        return;
    }

    _activeTool = _drawingTool;
    if (_activeTool == DrawingTool::Eraser) {
        beginEdit();
        _erasing = true;
        _lastEraserPosition = pagePosition;
        _editChanged |= _scene->eraseAt(pagePosition, kEraserRadius);
        event->accept();
        return;
    }
    if (_activeTool == DrawingTool::Pointer) {
        _pointing = true;
        _scene->showPointerAt(pagePosition);
        event->accept();
        return;
    }

    beginEdit();
    _strokeStart = pagePosition;
    QPainterPath path(pagePosition);
    // 极短首段使单次点击也形成可见圆头笔点。
    path.lineTo(pagePosition + QPointF(0.01, 0.0));
    const bool marker = _activeTool == DrawingTool::Marker;
    const QPen pen(marker ? ebThemeColor(EBThemeColor::BoardMarker)
                          : ebThemeColor(EBThemeColor::BoardPen),
                   marker ? kMarkerWidth : kPenWidth, Qt::SolidLine,
                   Qt::RoundCap, Qt::RoundJoin);
    _activeStroke = _scene->addStroke(path, pen);
    _editChanged = true;
    event->accept();
}

void EBBoardView::mouseMoveEvent(QMouseEvent *event)
{
    const QPointF scenePosition = mapToScene(event->pos());
    emit pagePositionChanged(scenePosition - pageRect().topLeft(),
                             pageRect().contains(scenePosition));

    if (_erasing && (event->buttons() & Qt::LeftButton)) {
        const QPointF current = boundedPagePosition(event->pos());
        eraseAlong(_lastEraserPosition, current);
        _lastEraserPosition = current;
        event->accept();
        return;
    }
    if (_pointing && (event->buttons() & Qt::LeftButton)) {
        if (_scene->pointerVisible())
            _scene->showPointerAt(scenePosition - pageRect().topLeft());
        event->accept();
        return;
    }
    if (_activeStroke && (event->buttons() & Qt::LeftButton)) {
        updateActiveStroke(boundedPagePosition(event->pos()));
        event->accept();
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
    if (_drawingTool == DrawingTool::Pan && (event->buttons() & Qt::LeftButton))
        _viewCenter = mapToScene(viewport()->rect().center());
}

void EBBoardView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && (_erasing || _pointing)) {
        if (_erasing)
            eraseAlong(_lastEraserPosition, boundedPagePosition(event->pos()));
        _erasing = false;
        _pointing = false;
        _scene->hidePointer();
        finishEdit();
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton && _activeStroke) {
        updateActiveStroke(boundedPagePosition(event->pos()));
        _activeStroke = nullptr;
        finishEdit();
        event->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
    if (_drawingTool == DrawingTool::Pan && event->button() == Qt::LeftButton)
        _viewCenter = mapToScene(viewport()->rect().center());
}

void EBBoardView::leaveEvent(QEvent *event)
{
    _scene->hidePointer();
    emit pagePositionChanged(QPointF(), false);
    QGraphicsView::leaveEvent(event);
}

void EBBoardView::hideEvent(QHideEvent *event)
{
    _viewCenter = mapToScene(viewport()->rect().center());
    // 模式切换可能发生在鼠标释放前，结束本次编辑以免保留失效图元指针。
    _activeStroke = nullptr;
    _erasing = false;
    _pointing = false;
    _scene->hidePointer();
    finishEdit();
    QGraphicsView::hideEvent(event);
}

void EBBoardView::fitPage()
{
    _zoomFactor = 1.0;
    _viewCenter = _scene->sceneRect().center();
    _viewStateReady = true;
    applyViewState();
}

void EBBoardView::applyViewState()
{
    if (!_viewStateReady || viewport()->width() <= 0 || viewport()->height() <= 0)
        return;
    resetTransform();
    fitInView(_scene->sceneRect(), Qt::KeepAspectRatio);
    scale(_zoomFactor, _zoomFactor);
    centerOn(_viewCenter);
}

void EBBoardView::zoomBy(qreal factor)
{
    const qreal next = qBound(kMinZoomFactor, _zoomFactor * factor, kMaxZoomFactor);
    if (qFuzzyCompare(next, _zoomFactor))
        return;
    _zoomFactor = next;
    applyViewState();
}

QPointF EBBoardView::boundedPagePosition(const QPointF &viewportPosition) const
{
    const QPointF position = toPagePosition(viewportPosition);
    return QPointF(qBound(0.0, position.x(), pageRect().width()),
                   qBound(0.0, position.y(), pageRect().height()));
}

void EBBoardView::updateActiveStroke(const QPointF &pagePosition)
{
    if (_activeTool == DrawingTool::Line) {
        // 直线每次从固定起点重建路径，不保留中途拖动点。
        QPainterPath line(_strokeStart);
        line.lineTo(pagePosition);
        _activeStroke->setPath(line);
    } else {
        QPainterPath path = _activeStroke->path();
        path.lineTo(pagePosition);
        _activeStroke->setPath(path);
    }
}

void EBBoardView::eraseAlong(const QPointF &from, const QPointF &to)
{
    // 相邻事件之间补点，快速拖动也能连续擦到中间笔迹。
    const int steps = qMax(1, qCeil(QLineF(from, to).length() / kEraserRadius));
    for (int index = 1; index <= steps; ++index) {
        const QPointF position = from + (to - from) * (qreal(index) / steps);
        _editChanged |= _scene->eraseAt(position, kEraserRadius);
    }
}

void EBBoardView::beginEdit()
{
    _editBefore = _scene->captureStrokes();
    _editActive = true;
    _editChanged = false;
    emit historyAvailabilityChanged(false, false);
}

void EBBoardView::finishEdit()
{
    if (!_editActive)
        return;
    _editActive = false;
    if (_editChanged) {
        const QString description = _activeTool == DrawingTool::Eraser
            ? tr("局部擦除") : tr("绘制笔迹");
        _undoStack->push(new StrokeEditCommand(this, _editBefore,
                                               _scene->captureStrokes(), description));
        syncCurrentPageStrokes();
    }
    _editBefore.clear();
    emit historyAvailabilityChanged(canUndo(), canRedo());
}

void EBBoardView::finishPageInteraction()
{
    _activeStroke = nullptr;
    _erasing = false;
    _pointing = false;
    _scene->hidePointer();
    finishEdit();
}

void EBBoardView::showCurrentPage()
{
    _undoStack->clear();
    _scene->showPage(*_document->currentPage());
    fitPage();
    emit historyAvailabilityChanged(false, false);
    emit pagePositionChanged(QPointF(), false);
}

void EBBoardView::syncCurrentPageStrokes()
{
    // 场景中的临时指示点不进入页面模型；只同步可恢复的笔迹。
    _document->currentPage()->setStrokes(_scene->captureStrokes());
    emit pageContentChanged(_document->currentPageIndex());
}
