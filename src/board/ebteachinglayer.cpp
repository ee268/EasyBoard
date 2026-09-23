#include "ebteachinglayer.h"

namespace {
int paintOrder(EBTeachingTools::Kind kind)
{
    if (kind == EBTeachingTools::Kind::Curtain
        || kind == EBTeachingTools::Kind::Spotlight)
        return 0;
    if (kind == EBTeachingTools::Kind::Magnifier)
        return 2;
    return 1;
}
}

void EBTeachingLayer::add(Kind kind, const QRectF &page)
{
    if (kind == Kind::None || _items.size() >= 32)
        return;
    EBTeachingTools tool;
    tool.setKind(kind, page);
    _items.append(tool);
    _active = _items.size() - 1;
}

void EBTeachingLayer::deactivate()
{
    _active = -1;
}

void EBTeachingLayer::removeActive()
{
    if (_active < 0 || _active >= _items.size())
        return;
    _items.removeAt(_active);
    _active = -1;
}

void EBTeachingLayer::clear()
{
    _items.clear();
    _active = -1;
}

EBTeachingLayer::Kind EBTeachingLayer::kind() const
{
    return _active >= 0 && _active < _items.size()
        ? _items.at(_active).kind() : Kind::None;
}

int EBTeachingLayer::count() const
{
    return _items.size();
}

QVector<EBTeachingState> EBTeachingLayer::states(const QRectF &page) const
{
    QVector<EBTeachingState> result;
    result.reserve(_items.size());
    for (const EBTeachingTools &tool : _items)
        result.append(tool.state(page));
    return result;
}

void EBTeachingLayer::restore(const QVector<EBTeachingState> &states,
                              const QRectF &page)
{
    clear();
    for (const EBTeachingState &state : states) {
        EBTeachingTools tool;
        tool.restore(state, page);
        _items.append(tool);
    }
}

bool EBTeachingLayer::press(const QPointF &position, const QRectF &page)
{
    for (int order = 2; order >= 0; --order) {
        for (int index = _items.size() - 1; index >= 0; --index) {
            if (paintOrder(_items.at(index).kind()) != order
                || !_items[index].press(position, page, index == _active))
                continue;
            _active = index;
            return true;
        }
    }
    return false;
}

void EBTeachingLayer::move(const QPointF &position, const QRectF &page)
{
    if (isInteracting())
        _items[_active].move(position, page);
}

QPainterPath EBTeachingLayer::release(const QPointF &position,
                                      const QRectF &page)
{
    if (!isInteracting())
        return QPainterPath();
    const QPainterPath path = _items[_active].release(position, page);
    if (_items[_active].closeRequested())
        removeActive();
    return path;
}

bool EBTeachingLayer::isInteracting() const
{
    return _active >= 0 && _active < _items.size()
        && _items.at(_active).isInteracting();
}

void EBTeachingLayer::cancel()
{
    if (isInteracting())
        _items[_active].cancel();
}

QPainterPath EBTeachingLayer::circlePath() const
{
    return kind() == Kind::Compass ? _items.at(_active).circlePath()
                                    : QPainterPath();
}

void EBTeachingLayer::flipActive(bool horizontal)
{
    if (kind() == Kind::Triangle45 || kind() == Kind::Triangle30)
        _items[_active].flip(horizontal);
}

void EBTeachingLayer::resetActive()
{
    if (kind() == Kind::Protractor)
        _items[_active].resetMeasurement();
}

void EBTeachingLayer::zoomMagnifier(qreal delta)
{
    if (kind() == Kind::Magnifier)
        _items[_active].zoomMagnifier(delta);
}

void EBTeachingLayer::toggleMagnifierShape()
{
    if (kind() == Kind::Magnifier)
        _items[_active].toggleMagnifierShape();
}

void EBTeachingLayer::paint(QPainter *painter, QGraphicsScene *scene,
                            const QRectF &page) const
{
    for (int order = 0; order <= 2; ++order) {
        for (int index = 0; index < _items.size(); ++index) {
            const EBTeachingTools &tool = _items.at(index);
            if (paintOrder(tool.kind()) == order)
                tool.paint(painter, scene, page, index == _active);
        }
    }
}
