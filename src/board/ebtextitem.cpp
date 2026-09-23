#include "ebtextitem.h"

#include <QTextDocument>
#include <QTextOption>
#include <QStyle>
#include <QStyleOptionGraphicsItem>
#include <QtMath>

namespace {
constexpr qreal kDefaultTextWidth = 360.0;
}

EBTextItem::EBTextItem(const QString &text, const QFont &font,
                       const QColor &color)
{
    setFont(font);
    setDefaultTextColor(color);
    setPlainText(text);
    setTextWidth(kDefaultTextWidth);
    QTextOption option = document()->defaultTextOption();
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    document()->setDefaultTextOption(option);
    refreshTransformOrigin();
}

void EBTextItem::paint(QPainter *painter,
                       const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    QStyleOptionGraphicsItem itemOption(*option);
    itemOption.state &= ~QStyle::State_Selected;
    QGraphicsTextItem::paint(painter, &itemOption, widget);
}

EBTextItem::State EBTextItem::state() const
{
    return {toPlainText(), font(), defaultTextColor(), pos(), zValue(),
            transformOriginPoint(), scale(), rotation(), textWidth(), _groupId,
            _locked};
}

void EBTextItem::applyState(const State &state)
{
    setFont(state.font);
    setDefaultTextColor(state.color);
    setPlainText(state.text);
    setTextWidth(state.textWidth);
    setPos(state.position);
    setZValue(state.zValue);
    setTransformOriginPoint(state.transformOrigin);
    setScale(state.scale);
    setRotation(state.rotation);
    _groupId = state.groupId;
    _locked = state.locked;
}

void EBTextItem::refreshTransformOrigin()
{
    if (qFuzzyCompare(scale(), 1.0) && qFuzzyIsNull(rotation()))
        setTransformOriginPoint(boundingRect().center());
}

QString EBTextItem::groupId() const
{
    return _groupId;
}

void EBTextItem::setGroupId(const QString &groupId)
{
    _groupId = groupId;
}

bool EBTextItem::isLocked() const
{
    return _locked;
}

void EBTextItem::setLocked(bool locked)
{
    _locked = locked;
}
