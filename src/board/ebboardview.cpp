#include "ebboardview.h"

#include <QPainter>
#include <QGraphicsLineItem>
#include <QResizeEvent>
#include <QRubberBand>
#include <QShowEvent>
#include <QUndoStack>
#include <QWheelEvent>
#include <QtMath>

#include "../domain/ebdocument.h"
#include "../core/ebsettings.h"
#include "../global/ebtheme.h"

namespace {
constexpr int kMaxHistoryEntries = 50;
constexpr qreal kZoomStep = 1.25;
constexpr qreal kMinZoomFactor = 0.5;
constexpr qreal kMaxZoomFactor = 4.0;
}

EBBoardView::EBBoardView(EBDocument *document, QWidget *parent)
    : QGraphicsView(parent)
    , _document(document)
    , _scene(new EBBoardScene(this))
    , _activeStroke(nullptr)
    , _selectionBand(new QRubberBand(QRubberBand::Rectangle, viewport()))
    , _selectionModifiers(Qt::NoModifier)
    , _selectingArea(false)
    , _keyboardMoving(false)
    , _editingText(nullptr)
    , _editingTextWasNew(false)
    , _drawingTool(DrawingTool::Pen)
    , _activeTool(DrawingTool::Pen)
    , _verticalGuide(nullptr)
    , _horizontalGuide(nullptr)
    , _guidePreview(nullptr)
    , _guideAxis(GuideAxis::None)
    , _guideIndex(-1)
    , _guideValue(0.0)
    , _transformHandle(TransformHandle::None)
    , _snapEnabled(true)
    , _gridSnapEnabled(false)
    , _penColor(EBSettings::settings()->penColor())
    , _markerColor(EBSettings::settings()->markerColor())
    , _penWidth(EBSettings::settings()->penWidth())
    , _markerWidth(EBSettings::settings()->markerWidth())
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
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    QPen guidePen(ebThemeColor(EBThemeColor::BoardAlignmentGuide), 1.0,
                  Qt::DashLine);
    guidePen.setCosmetic(true);
    _verticalGuide = _scene->addLine(QLineF(), guidePen);
    _horizontalGuide = _scene->addLine(QLineF(), guidePen);
    _verticalGuide->setZValue(1000001.0);
    _horizontalGuide->setZValue(1000001.0);
    _verticalGuide->setAcceptedMouseButtons(Qt::NoButton);
    _horizontalGuide->setAcceptedMouseButtons(Qt::NoButton);
    QPen manualPen(ebThemeColor(EBThemeColor::BoardManualGuide), 1.0,
                   Qt::DashLine);
    manualPen.setCosmetic(true);
    _guidePreview = _scene->addLine(QLineF(), manualPen);
    _guidePreview->setZValue(1000000.5);
    _guidePreview->setAcceptedMouseButtons(Qt::NoButton);
    _guidePreview->hide();
    refreshManualGuides();
    hideSnapGuides();
    setBackgroundBrush(ebThemeColor(EBThemeColor::BoardBackground));
    setRenderHint(QPainter::Antialiasing);
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setMouseTracking(true);
    viewport()->setMouseTracking(true);
    connect(_scene, &QGraphicsScene::selectionChanged, this, [this]() {
        emit selectionAvailabilityChanged(hasSelectedObject());
        viewport()->update();
    });
    connect(_scene, &QGraphicsScene::focusItemChanged, this,
            [this](QGraphicsItem *newFocus, QGraphicsItem *oldFocus,
                   Qt::FocusReason) {
        if (_editingText && oldFocus == _editingText
            && newFocus != _editingText)
            finishTextEditing();
    });
    fitPage();
}

void EBBoardView::setDrawingTool(DrawingTool tool)
{
    if (_teachingTools.kind() != EBTeachingTools::Kind::None) {
        _teachingTools.setKind(EBTeachingTools::Kind::None, pageRect());
        emit teachingToolChanged(EBTeachingTools::Kind::None);
        viewport()->update();
    }
    finishGuideDrag(false);
    finishObjectTransform();
    finishKeyboardMove();
    finishAreaSelection();
    finishTextEditing();
    if (!_movingObjects.isEmpty())
        finishObjectMove();
    // 切换只影响下一次操作，进行中的笔迹继续使用按下时的工具。
    _drawingTool = tool;
    _scene->setObjectInteractionEnabled(tool == DrawingTool::Select
                                        || tool == DrawingTool::Text);
    // 平移只改变视图，不建立笔迹或撤销历史。
    setDragMode(tool == DrawingTool::Pan ? QGraphicsView::ScrollHandDrag
                                         : QGraphicsView::NoDrag);
    emit drawingToolChanged(tool);
}

EBBoardView::DrawingTool EBBoardView::drawingTool() const
{
    return _drawingTool;
}

void EBBoardView::setSnapEnabled(bool enabled)
{
    _snapEnabled = enabled;
    if (!enabled)
        hideSnapGuides();
}

bool EBBoardView::snapEnabled() const
{
    return _snapEnabled;
}

void EBBoardView::setGridSnapEnabled(bool enabled)
{
    _gridSnapEnabled = enabled;
}

bool EBBoardView::gridSnapEnabled() const
{
    return _gridSnapEnabled;
}

QColor EBBoardView::penColor() const { return _penColor; }
QColor EBBoardView::markerColor() const { return _markerColor; }
qreal EBBoardView::penWidth() const { return _penWidth; }
qreal EBBoardView::markerWidth() const { return _markerWidth; }

void EBBoardView::setPenColor(const QColor &color)
{
    if (!color.isValid())
        return;
    _penColor = color;
    EBSettings::settings()->setPenColor(color);
    EBSettings::settings()->save();
}

void EBBoardView::setMarkerColor(const QColor &color)
{
    if (!color.isValid())
        return;
    _markerColor = color;
    EBSettings::settings()->setMarkerColor(color);
    EBSettings::settings()->save();
}

void EBBoardView::setPenWidth(qreal width)
{
    if (!qIsFinite(width))
        return;
    _penWidth = qBound(1.0, width, 24.0);
    EBSettings::settings()->setPenWidth(_penWidth);
    EBSettings::settings()->save();
}

void EBBoardView::setMarkerWidth(qreal width)
{
    if (!qIsFinite(width))
        return;
    _markerWidth = qBound(4.0, width, 48.0);
    EBSettings::settings()->setMarkerWidth(_markerWidth);
    EBSettings::settings()->save();
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
    _movingObjects.clear();
    _moveStartPositions.clear();
    _snapReferences.clear();
    hideSnapGuides();
    finishGuideDrag(false);
    _transformHandle = TransformHandle::None;
    _transformStates.clear();
    _editingText = nullptr;
    _editingTextWasNew = false;
    _erasing = false;
    _pointing = false;
    _editActive = false;
    _editChanged = false;
    _editBefore = Snapshot();
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
    if (_teachingTools.kind() != EBTeachingTools::Kind::None) {
        _teachingTools.setKind(EBTeachingTools::Kind::None, pageRect());
        emit teachingToolChanged(EBTeachingTools::Kind::None);
    }
    _document->currentPage()->setSize(size);
    _scene->setPageSize(size);
    _scene->setGuides(_document->currentPage()->horizontalGuides(),
                      _document->currentPage()->verticalGuides());
    refreshManualGuides();
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
    viewport()->update();
    event->accept();
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
