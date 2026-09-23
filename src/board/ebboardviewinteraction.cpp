#include "ebboardview.h"

#include <QHideEvent>
#include <QKeyEvent>
#include <QLineF>
#include <QMouseEvent>
#include <QRubberBand>
#include <QSet>
#include <QTextCursor>
#include <QtMath>

#include "../global/ebtheme.h"

namespace {
constexpr qreal kEraserRadius = 12.0;
}

void EBBoardView::keyPressEvent(QKeyEvent *event)
{
    if (_editingText
        && (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
        && (event->modifiers() & Qt::ControlModifier)) {
        finishTextEditing();
        setFocus();
        event->accept();
        return;
    }
    const bool arrow = event->key() == Qt::Key_Left
        || event->key() == Qt::Key_Right
        || event->key() == Qt::Key_Up
        || event->key() == Qt::Key_Down;
    const bool blockedModifier = event->modifiers()
        & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier);
    if (!_editingText && arrow && !blockedModifier
        && _scene->selectedObjectsEditable()) {
        const qreal distance = event->modifiers() & Qt::ShiftModifier
            ? 10.0 : 1.0;
        QPointF delta;
        if (event->key() == Qt::Key_Left)
            delta.setX(-distance);
        else if (event->key() == Qt::Key_Right)
            delta.setX(distance);
        else if (event->key() == Qt::Key_Up)
            delta.setY(-distance);
        else
            delta.setY(distance);
        nudgeSelectedObjects(delta);
        event->accept();
        return;
    }
    QGraphicsView::keyPressEvent(event);
}

void EBBoardView::keyReleaseEvent(QKeyEvent *event)
{
    const bool arrow = event->key() == Qt::Key_Left
        || event->key() == Qt::Key_Right
        || event->key() == Qt::Key_Up
        || event->key() == Qt::Key_Down;
    if (_keyboardMoving && arrow && !event->isAutoRepeat()) {
        finishKeyboardMove();
        event->accept();
        return;
    }
    QGraphicsView::keyReleaseEvent(event);
}

void EBBoardView::mousePressEvent(QMouseEvent *event)
{
    finishKeyboardMove();
    if (event->button() == Qt::LeftButton
        && (event->pos().x() < 24 || event->pos().y() < 24)) {
        if (event->pos().x() < 24 && event->pos().y() < 24) {
            event->accept();
            return;
        }
        finishTextEditing();
        finishAreaSelection();
        finishObjectMove();
        const GuideAxis axis = event->pos().y() < 24
            ? GuideAxis::Horizontal : GuideAxis::Vertical;
        startGuideDrag(axis, -1, 0.0);
        event->accept();
        return;
    }
    if (_drawingTool == DrawingTool::Select
        && (event->button() == Qt::LeftButton
            || event->button() == Qt::RightButton)
        && !(event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier))) {
        GuideAxis axis = GuideAxis::None;
        int index = -1;
        if (!_scene->objectAt(mapToScene(event->pos()))
            && guideAt(event->pos(), &axis, &index)) {
            if (event->button() == Qt::RightButton)
                removeGuide(axis, index);
            else {
                const qreal value = axis == GuideAxis::Horizontal
                    ? _scene->horizontalGuides().at(index)
                    : _scene->verticalGuides().at(index);
                startGuideDrag(axis, index, value);
            }
            event->accept();
            return;
        }
    }
    if (_drawingTool == DrawingTool::Select
        && event->button() == Qt::LeftButton) {
        const TransformHandle handle = transformHandleAt(event->pos());
        if (handle != TransformHandle::None) {
            startObjectTransform(handle, mapToScene(event->pos()));
            event->accept();
            return;
        }
    }
    if (_drawingTool == DrawingTool::Pan) {
        QGraphicsView::mousePressEvent(event);
        return;
    }
    const QPointF scenePosition = mapToScene(event->pos());
    const QPointF pagePosition = scenePosition - pageRect().topLeft();
    if (_drawingTool == DrawingTool::Text
        && event->button() == Qt::LeftButton
        && pageRect().contains(scenePosition)) {
        EBTextItem *text = _scene->textAt(scenePosition);
        if (text && _scene->isObjectLocked(text)) {
            finishTextEditing();
            _scene->clearSelection();
            _scene->setObjectSelected(text, true);
            event->accept();
            return;
        }
        if (_editingText == text && text) {
            QGraphicsView::mousePressEvent(event);
            return;
        }
        finishTextEditing();
        beginEdit();
        const bool newlyCreated = !text;
        if (!text) {
            QFont font;
            font.setPointSize(22);
            text = _scene->addText(QString(), font,
                                   ebThemeColor(EBThemeColor::BoardPen));
            text->setPos(scenePosition);
        }
        startTextEditing(text, newlyCreated);
        event->accept();
        return;
    }
    if (_drawingTool == DrawingTool::Text) {
        QGraphicsView::mousePressEvent(event);
        return;
    }
    if (_drawingTool == DrawingTool::Select
        && event->button() == Qt::LeftButton) {
        QGraphicsItem *object = pageRect().contains(scenePosition)
            ? _scene->objectAt(scenePosition) : nullptr;
        const bool extendSelection = event->modifiers()
            & (Qt::ControlModifier | Qt::ShiftModifier);
        if (!object) {
            if (pageRect().contains(scenePosition))
                startAreaSelection(event->pos(), event->modifiers());
            else if (!extendSelection)
                _scene->clearSelection();
            event->accept();
            return;
        }
        if (extendSelection) {
            _scene->setObjectSelected(object, !object->isSelected());
            event->accept();
            return;
        }
        if (!object->isSelected()) {
            _scene->clearSelection();
            _scene->setObjectSelected(object, true);
        }
        if (!_scene->selectedObjectsEditable()) {
            event->accept();
            return;
        }
        _activeTool = DrawingTool::Select;
        _movingObjects = _scene->selectedObjects();
        _moveStartPositions.clear();
        for (QGraphicsItem *selected : _movingObjects)
            _moveStartPositions.append(selected->pos());
        _snapReferences = _scene->objectReferenceBounds();
        _moveStartScene = scenePosition;
        beginEdit();
        event->accept();
        return;
    }
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
    const QPen pen(marker ? _markerColor : _penColor,
                   marker ? _markerWidth : _penWidth, Qt::SolidLine,
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

    if (_guideAxis != GuideAxis::None
        && (event->buttons() & Qt::LeftButton)) {
        updateGuideDrag(scenePosition);
        event->accept();
        return;
    }
    if (_transformHandle != TransformHandle::None
        && (event->buttons() & Qt::LeftButton)) {
        updateObjectTransform(scenePosition);
        event->accept();
        return;
    }

    if (_selectingArea && (event->buttons() & Qt::LeftButton)) {
        updateAreaSelection(event->pos());
        event->accept();
        return;
    }

    if (!_movingObjects.isEmpty() && (event->buttons() & Qt::LeftButton)) {
        updateObjectMove(scenePosition,
                         !(event->modifiers() & Qt::AltModifier));
        event->accept();
        return;
    }
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
    if (event->button() == Qt::LeftButton
        && _transformHandle != TransformHandle::None) {
        updateObjectTransform(mapToScene(event->pos()));
        finishObjectTransform();
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton
        && _guideAxis != GuideAxis::None) {
        const QPointF position = mapToScene(event->pos());
        updateGuideDrag(position);
        finishGuideDrag(pageRect().contains(position));
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton && _selectingArea) {
        updateAreaSelection(event->pos());
        finishAreaSelection();
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton && !_movingObjects.isEmpty()) {
        updateObjectMove(mapToScene(event->pos()),
                         !(event->modifiers() & Qt::AltModifier));
        finishObjectMove();
        event->accept();
        return;
    }
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
    finishTextEditing();
    finishAreaSelection();
    finishKeyboardMove();
    finishGuideDrag(false);
    finishObjectTransform();
    finishObjectMove();
    _activeStroke = nullptr;
    _erasing = false;
    _pointing = false;
    _scene->hidePointer();
    finishEdit();
    QGraphicsView::hideEvent(event);
}

void EBBoardView::updateActiveStroke(const QPointF &pagePosition)
{
    if (_activeTool == DrawingTool::Line
        || _activeTool == DrawingTool::Rectangle
        || _activeTool == DrawingTool::Ellipse
        || _activeTool == DrawingTool::Arrow) {
        // 直线每次从固定起点重建路径，不保留中途拖动点。
        QPainterPath shape;
        const QPointF end = pagePosition == _strokeStart
            ? pagePosition + QPointF(0.01, 0.01) : pagePosition;
        if (_activeTool == DrawingTool::Rectangle)
            shape.addRect(QRectF(_strokeStart, end).normalized());
        else if (_activeTool == DrawingTool::Ellipse)
            shape.addEllipse(QRectF(_strokeStart, end).normalized());
        else {
            shape.moveTo(_strokeStart);
            shape.lineTo(end);
            if (_activeTool == DrawingTool::Arrow) {
                const QLineF shaft(_strokeStart, end);
                const qreal wing = qMin(18.0, shaft.length() * 0.35);
                const qreal angle = qAtan2(end.y() - _strokeStart.y(),
                                           end.x() - _strokeStart.x());
                shape.moveTo(end);
                shape.lineTo(end - QPointF(wing * qCos(angle - 0.55),
                                           wing * qSin(angle - 0.55)));
                shape.moveTo(end);
                shape.lineTo(end - QPointF(wing * qCos(angle + 0.55),
                                           wing * qSin(angle + 0.55)));
            }
        }
        _activeStroke->setPath(shape);
    } else {
        QPainterPath path = _activeStroke->path();
        path.lineTo(pagePosition);
        _activeStroke->setPath(path);
    }
    // 新笔迹尚未发生变换时，持续以当前几何中心作为后续缩放和旋转中心。
    _activeStroke->setTransformOriginPoint(
        _activeStroke->path().boundingRect().center());
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

void EBBoardView::startAreaSelection(
    const QPoint &viewportPosition, Qt::KeyboardModifiers modifiers)
{
    _selectingArea = true;
    _selectionOrigin = viewportPosition;
    _selectionModifiers = modifiers;
    _selectionBaseline = _scene->selectedObjects();
    _selectionBand->setGeometry(QRect(_selectionOrigin, QSize()));
    _selectionBand->show();
    updateAreaSelection(viewportPosition);
}

void EBBoardView::updateAreaSelection(const QPoint &viewportPosition)
{
    if (!_selectingArea)
        return;
    const QRect viewportRect = QRect(_selectionOrigin, viewportPosition)
        .normalized().intersected(viewport()->rect());
    _selectionBand->setGeometry(viewportRect);
    const QRectF sceneRect = mapToScene(viewportRect).boundingRect()
        .intersected(pageRect());
    const QVector<QGraphicsItem *> hits = sceneRect.isEmpty()
        ? QVector<QGraphicsItem *>() : _scene->objectsInRect(sceneRect);

    QSet<QGraphicsItem *> desired;
    const bool toggle = _selectionModifiers & Qt::ControlModifier;
    const bool extend = _selectionModifiers & Qt::ShiftModifier;
    if (toggle || extend) {
        for (QGraphicsItem *object : _selectionBaseline)
            desired.insert(object);
    }
    for (QGraphicsItem *object : hits) {
        if (toggle && desired.contains(object))
            desired.remove(object);
        else
            desired.insert(object);
    }
    _scene->clearSelection();
    for (QGraphicsItem *object : desired)
        object->setSelected(true);
}

void EBBoardView::finishAreaSelection()
{
    if (!_selectingArea)
        return;
    _selectionBand->hide();
    _selectionBaseline.clear();
    _selectionModifiers = Qt::NoModifier;
    _selectingArea = false;
}

void EBBoardView::nudgeSelectedObjects(const QPointF &delta)
{
    const QVector<QGraphicsItem *> objects = _scene->selectedObjects();
    if (!_scene->selectedObjectsEditable()
        || (qFuzzyIsNull(delta.x()) && qFuzzyIsNull(delta.y())))
        return;
    if (!_keyboardMoving) {
        _activeTool = DrawingTool::Select;
        beginEdit();
        _keyboardMoving = true;
    }
    QVector<QPointF> before;
    before.reserve(objects.size());
    for (QGraphicsItem *object : objects) {
        before.append(object->pos());
        object->moveBy(delta.x(), delta.y());
    }
    keepObjectsInsidePage(objects);
    for (int index = 0; index < objects.size(); ++index)
        _editChanged |= objects.at(index)->pos() != before.at(index);
}

void EBBoardView::finishKeyboardMove()
{
    if (!_keyboardMoving)
        return;
    _keyboardMoving = false;
    finishEdit();
}

void EBBoardView::startTextEditing(EBTextItem *text, bool newlyCreated)
{
    if (!text)
        return;
    _activeTool = DrawingTool::Text;
    _editingText = text;
    _editingTextWasNew = newlyCreated;
    _textBeforeEdit = text->toPlainText();
    _scene->clearSelection();
    text->setSelected(true);
    text->setTextInteractionFlags(Qt::TextEditorInteraction);
    text->setFocus(Qt::MouseFocusReason);
    QTextCursor cursor = text->textCursor();
    cursor.movePosition(QTextCursor::End);
    text->setTextCursor(cursor);
}

void EBBoardView::finishTextEditing()
{
    if (!_editingText)
        return;
    EBTextItem *text = _editingText;
    const bool newlyCreated = _editingTextWasNew;
    const QString before = _textBeforeEdit;
    const QString after = text->toPlainText();
    _editingText = nullptr;
    _editingTextWasNew = false;
    _textBeforeEdit.clear();
    text->setTextInteractionFlags(Qt::NoTextInteraction);
    text->clearFocus();
    if (after.isEmpty()) {
        delete text;
        _editChanged = !newlyCreated;
    } else {
        text->refreshTransformOrigin();
        keepObjectInsidePage(text);
        _editChanged = newlyCreated || after != before;
    }
    finishEdit();
    emit selectionAvailabilityChanged(hasSelectedObject());
}

void EBBoardView::keepObjectInsidePage(QGraphicsItem *object)
{
    if (!object)
        return;
    const QRectF page = pageRect();
    QRectF bounds = object->sceneBoundingRect();
    if (bounds.width() > page.width() || bounds.height() > page.height()) {
        const qreal fit = qMin(page.width() / bounds.width(),
                               page.height() / bounds.height());
        object->setScale(object->scale() * fit);
        bounds = object->sceneBoundingRect();
    }
    QPointF offset;
    if (bounds.width() <= page.width()) {
        if (bounds.left() < page.left())
            offset.rx() = page.left() - bounds.left();
        else if (bounds.right() > page.right())
            offset.rx() = page.right() - bounds.right();
    }
    if (bounds.height() <= page.height()) {
        if (bounds.top() < page.top())
            offset.ry() = page.top() - bounds.top();
        else if (bounds.bottom() > page.bottom())
            offset.ry() = page.bottom() - bounds.bottom();
    }
    object->moveBy(offset.x(), offset.y());
}

void EBBoardView::keepObjectsInsidePage(
    const QVector<QGraphicsItem *> &objects)
{
    if (objects.isEmpty())
        return;
    QRectF bounds;
    const QRectF page = pageRect();
    for (QGraphicsItem *object : objects) {
        const QRectF objectBounds = object->sceneBoundingRect();
        if (objectBounds.width() > page.width()
            || objectBounds.height() > page.height())
            keepObjectInsidePage(object);
        bounds = bounds.isNull() ? object->sceneBoundingRect()
                                 : bounds.united(object->sceneBoundingRect());
    }
    QPointF offset;
    if (bounds.width() <= page.width()) {
        if (bounds.left() < page.left())
            offset.rx() = page.left() - bounds.left();
        else if (bounds.right() > page.right())
            offset.rx() = page.right() - bounds.right();
    }
    if (bounds.height() <= page.height()) {
        if (bounds.top() < page.top())
            offset.ry() = page.top() - bounds.top();
        else if (bounds.bottom() > page.bottom())
            offset.ry() = page.bottom() - bounds.bottom();
    }
    for (QGraphicsItem *object : objects)
        object->moveBy(offset.x(), offset.y());
}


