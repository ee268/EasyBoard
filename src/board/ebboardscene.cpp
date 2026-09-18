#include "ebboardscene.h"

#include <QGraphicsEllipseItem>
#include <QGraphicsRectItem>
#include <QImage>
#include <QPainter>
#include <QPixmap>

#include "../global/ebtheme.h"

namespace {
// 页面尺寸和边距属于场景；视图只负责把视口坐标换算为页面坐标。
constexpr qreal kPageMargin = 80.0;
constexpr qreal kPageWidth = 1200.0;
constexpr qreal kPageHeight = 900.0;
constexpr qreal kPointerRadius = 7.0;
constexpr int kPatternSpacing = 40;
}

EBBoardScene::EBBoardScene(QObject *parent)
    : QGraphicsScene(parent)
    , _pageRect(kPageMargin, kPageMargin, kPageWidth, kPageHeight)
    , _pageItem(nullptr)
    , _pointerItem(nullptr)
    , _pageColor(PageColor::White)
    , _pagePattern(PagePattern::Blank)
{
    setSceneRect(0.0, 0.0,
                 kPageWidth + 2.0 * kPageMargin,
                 kPageHeight + 2.0 * kPageMargin);
    _pageItem = addRect(_pageRect, QPen(ebThemeColor(EBThemeColor::BoardBorder)),
                         QBrush(ebThemeColor(EBThemeColor::BoardWhite)));
    refreshPageBackground();

    // 红点是临时指示图元，不属于持久笔迹和撤销快照。
    _pointerItem = addEllipse(-kPointerRadius, -kPointerRadius,
                              2.0 * kPointerRadius, 2.0 * kPointerRadius,
                              QPen(ebThemeColor(EBThemeColor::BoardPointerBorder), 2.0),
                              QBrush(ebThemeColor(EBThemeColor::BoardPointerFill)));
    _pointerItem->setZValue(2.0);
    _pointerItem->hide();
}

QRectF EBBoardScene::pageRect() const
{
    return _pageRect;
}

void EBBoardScene::showPage(const EBPage &page)
{
    hidePointer();
    setPageColor(page.color());
    setPagePattern(page.pattern());
    restoreStrokes(page.strokes());
}

void EBBoardScene::setPageColor(PageColor color)
{
    if (_pageColor == color)
        return;
    _pageColor = color;
    refreshPageBackground();
}

EBBoardScene::PageColor EBBoardScene::pageColor() const
{
    return _pageColor;
}

void EBBoardScene::setPagePattern(PagePattern pattern)
{
    if (_pagePattern == pattern)
        return;
    _pagePattern = pattern;
    refreshPageBackground();
}

EBBoardScene::PagePattern EBBoardScene::pagePattern() const
{
    return _pagePattern;
}

EBStrokeItem *EBBoardScene::addStroke(const QPainterPath &path, const QPen &pen)
{
    EBStrokeItem *stroke = new EBStrokeItem(path, pen);
    addItem(stroke);
    stroke->setPos(_pageRect.topLeft());
    stroke->setZValue(1.0);
    return stroke;
}

bool EBBoardScene::eraseAt(const QPointF &pagePosition, qreal radius)
{
    const QPointF scenePosition = pagePosition + _pageRect.topLeft();
    const QRectF touchArea(scenePosition.x() - radius, scenePosition.y() - radius,
                           2.0 * radius, 2.0 * radius);
    bool changed = false;
    // 只裁切自有笔迹图元；页面背景与指示点不会被橡皮影响。
    for (QGraphicsItem *item : items(touchArea, Qt::IntersectsItemShape)) {
        auto *stroke = dynamic_cast<EBStrokeItem *>(item);
        if (!stroke)
            continue;
        QVector<QPainterPath> remaining;
        const qreal cutRadius = radius + stroke->pen().widthF() / 2.0;
        if (!stroke->splitAt(stroke->mapFromScene(scenePosition), cutRadius, remaining))
            continue;

        changed = true;
        const EBStrokeItem::State original = stroke->state();
        delete stroke;
        for (const QPainterPath &part : remaining) {
            EBStrokeItem *kept = addStroke(part, original.pen);
            EBStrokeItem::State state = original;
            state.path = part;
            kept->applyState(state);
        }
    }
    return changed;
}

EBBoardScene::Snapshot EBBoardScene::captureStrokes() const
{
    Snapshot snapshot;
    // 从底到顶保存，以便撤销命令重建时保留笔迹的覆盖顺序。
    for (QGraphicsItem *item : items(Qt::AscendingOrder)) {
        if (auto *stroke = dynamic_cast<EBStrokeItem *>(item))
            snapshot.append(stroke->state());
    }
    return snapshot;
}

void EBBoardScene::restoreStrokes(const Snapshot &snapshot)
{
    for (QGraphicsItem *item : items()) {
        if (auto *stroke = dynamic_cast<EBStrokeItem *>(item))
            delete stroke;
    }
    for (const EBStrokeItem::State &state : snapshot) {
        EBStrokeItem *stroke = addStroke(state.path, state.pen);
        stroke->applyState(state);
    }
}

void EBBoardScene::showPointerAt(const QPointF &pagePosition)
{
    _pointerItem->setPos(pagePosition + _pageRect.topLeft());
    _pointerItem->show();
}

void EBBoardScene::hidePointer()
{
    _pointerItem->hide();
}

bool EBBoardScene::pointerVisible() const
{
    return _pointerItem->isVisible();
}

void EBBoardScene::refreshPageBackground()
{
    // 各页按自身选择读取主题色；缩略图场景不修改全局主题。
    const QColor base = _pageColor == PageColor::White
        ? ebThemeColor(EBThemeColor::BoardWhite)
        : ebThemeColor(EBThemeColor::BoardCream);
    if (_pagePattern == PagePattern::Blank) {
        _pageItem->setBrush(QBrush(base));
        return;
    }

    // 底纹由代码绘制，保持现有 EasyBoard 页面效果。
    QImage tile(kPatternSpacing, kPatternSpacing, QImage::Format_ARGB32_Premultiplied);
    tile.fill(base);
    QPainter painter(&tile);
    const QColor patternColor = _pageColor == PageColor::White
        ? ebThemeColor(EBThemeColor::BoardWhitePattern)
        : ebThemeColor(EBThemeColor::BoardCreamPattern);
    painter.setPen(QPen(patternColor, 1.0));
    if (_pagePattern == PagePattern::Grid) {
        painter.drawLine(0, 0, kPatternSpacing - 1, 0);
        painter.drawLine(0, 0, 0, kPatternSpacing - 1);
    } else {
        painter.drawLine(0, kPatternSpacing - 1,
                         kPatternSpacing - 1, kPatternSpacing - 1);
    }
    painter.end();
    _pageItem->setBrush(QBrush(QPixmap::fromImage(tile)));
}
