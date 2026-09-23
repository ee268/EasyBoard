#include "ebboardview.h"

#include <QTransform>

void EBBoardView::setTeachingTool(EBTeachingTools::Kind kind)
{
    if (kind == EBTeachingTools::Kind::None) {
        _teachingTools.setKind(kind, pageRect());
    } else {
        finishPageInteraction();
        setDrawingTool(DrawingTool::Select);
        _scene->clearSelection();
        _teachingTools.setKind(kind, pageRect());
    }
    emit teachingToolChanged(kind);
    viewport()->update();
}

EBTeachingTools::Kind EBBoardView::teachingTool() const
{
    return _teachingTools.kind();
}

void EBBoardView::drawCompassCircle()
{
    addTeachingStroke(_teachingTools.circlePath(), tr("圆规画圆"));
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
