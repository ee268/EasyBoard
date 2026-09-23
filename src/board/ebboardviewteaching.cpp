#include "ebboardview.h"

#include <QTransform>
#include <QPainter>

#include "../domain/ebdocument.h"

void EBBoardView::setTeachingTool(EBTeachingTools::Kind kind)
{
    if (kind == EBTeachingTools::Kind::None) {
        _teachingTools.removeActive();
    } else {
        finishPageInteraction();
        setDrawingTool(DrawingTool::Select);
        _scene->clearSelection();
        _teachingTools.add(kind, pageRect());
    }
    syncTeachingTools();
    emit teachingToolChanged(_teachingTools.kind());
    viewport()->update();
}

EBTeachingTools::Kind EBBoardView::teachingTool() const
{
    return _teachingTools.kind();
}

int EBBoardView::teachingToolCount() const
{
    return _teachingTools.count();
}

void EBBoardView::drawCompassCircle()
{
    addTeachingStroke(_teachingTools.circlePath(), tr("圆规画圆"));
}

void EBBoardView::flipTeachingTool(bool horizontal)
{
    _teachingTools.flipActive(horizontal);
    syncTeachingTools();
    viewport()->update();
}

void EBBoardView::resetTeachingTool()
{
    _teachingTools.resetActive();
    syncTeachingTools();
    viewport()->update();
}

void EBBoardView::zoomTeachingMagnifier(qreal delta)
{
    _teachingTools.zoomMagnifier(delta);
    syncTeachingTools();
    viewport()->update();
}

void EBBoardView::toggleTeachingMagnifierShape()
{
    _teachingTools.toggleMagnifierShape();
    syncTeachingTools();
    viewport()->update();
}

void EBBoardView::syncTeachingTools()
{
    _document->currentPage()->setTeachingTools(
        _teachingTools.states(pageRect()));
    emit pageContentChanged(_document->currentPageIndex());
}

void EBBoardView::paintTeachingTools(QPainter *painter) const
{
    _teachingTools.paint(painter, _scene, pageRect());
}

void EBBoardView::addTeachingStroke(const QPainterPath &scenePath,
                                    const QString &description)
{
    if (scenePath.isEmpty())
        return;
    const QRectF page = pageRect();
    const QPainterPath pagePath = QTransform::fromTranslate(
        -page.left(), -page.top()).map(scenePath);
    beginEdit();
    _editDescription = description;
    _activeTool = DrawingTool::Line;
    const QPen pen(_penColor, _penWidth, Qt::SolidLine,
                   Qt::RoundCap, Qt::RoundJoin);
    EBStrokeItem *stroke = _scene->addStroke(pagePath, pen);
    stroke->setTransformOriginPoint(pagePath.boundingRect().center());
    _editChanged = true;
    finishEdit();
}
