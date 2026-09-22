#include "ebboardscene.h"

#include <QGraphicsEllipseItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsRectItem>
#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QtMath>

#include "../global/ebtheme.h"

namespace {
// 页面尺寸和边距属于场景；视图只负责把视口坐标换算为页面坐标。
constexpr qreal kPageMargin = 80.0;
constexpr qreal kStandardPageWidth = 1200.0;
constexpr qreal kWidescreenPageWidth = 1600.0;
constexpr qreal kPageHeight = 900.0;
constexpr qreal kPointerRadius = 7.0;
constexpr int kPatternSpacing = 40;
}

EBBoardScene::EBBoardScene(QObject *parent)
    : QGraphicsScene(parent)
    , _pageRect(kPageMargin, kPageMargin, kStandardPageWidth, kPageHeight)
    , _pageItem(nullptr)
    , _pageImageItem(nullptr)
    , _pointerItem(nullptr)
    , _pageColor(PageColor::White)
    , _pagePattern(PagePattern::Blank)
    , _pageSize(PageSize::Standard)
    , _objectInteractionEnabled(false)
{
    setSceneRect(0.0, 0.0, kStandardPageWidth + 2.0 * kPageMargin,
                 kPageHeight + 2.0 * kPageMargin);
    _pageItem = addRect(_pageRect, QPen(ebThemeColor(EBThemeColor::BoardBorder)),
                         QBrush(ebThemeColor(EBThemeColor::BoardWhite)));
    refreshPageBackground();

    // 导入图片位于页面底纹之上、笔迹之下，不参与擦除和撤销。
    _pageImageItem = addPixmap(QPixmap());
    _pageImageItem->setZValue(0.5);
    _pageImageItem->hide();

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
    setPageSize(page.size());
    setPageColor(page.color());
    setPagePattern(page.pattern());
    _pageBackgroundImage = page.backgroundImage();
    refreshPageImage();
    restoreSnapshot({page.strokes(), page.texts(), page.images()});
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

void EBBoardScene::setPageSize(PageSize size)
{
    if (_pageSize == size)
        return;
    _pageSize = size;
    refreshPageGeometry();
}

EBBoardScene::PageSize EBBoardScene::pageSize() const
{
    return _pageSize;
}

EBStrokeItem *EBBoardScene::addStroke(const QPainterPath &path, const QPen &pen)
{
    EBStrokeItem *stroke = new EBStrokeItem(path, pen);
    stroke->setFlag(QGraphicsItem::ItemIsSelectable,
                    _objectInteractionEnabled);
    addItem(stroke);
    stroke->setPos(_pageRect.topLeft());
    stroke->setZValue(1.0);
    return stroke;
}

EBTextItem *EBBoardScene::addText(const QString &text, const QFont &font,
                                  const QColor &color)
{
    EBTextItem *item = new EBTextItem(text, font, color);
    item->setFlag(QGraphicsItem::ItemIsSelectable,
                  _objectInteractionEnabled);
    item->setTextInteractionFlags(Qt::NoTextInteraction);
    addItem(item);
    item->setZValue(1.1);
    return item;
}

EBImageItem *EBBoardScene::addImage(const EBImageItem::State &state)
{
    EBImageItem *item = new EBImageItem(state);
    if (!item->isValid()) {
        delete item;
        return nullptr;
    }
    item->setFlag(QGraphicsItem::ItemIsSelectable,
                  _objectInteractionEnabled);
    addItem(item);
    return item;
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
            if (qFuzzyCompare(original.scale, 1.0)
                && qFuzzyIsNull(original.rotation))
                state.transformOrigin = part.boundingRect().center();
            kept->applyState(state);
        }
    }
    return changed;
}

EBBoardScene::StrokeSnapshot EBBoardScene::captureStrokes() const
{
    StrokeSnapshot snapshot;
    // 从底到顶保存，以便撤销命令重建时保留笔迹的覆盖顺序。
    for (QGraphicsItem *item : items(Qt::AscendingOrder)) {
        if (auto *stroke = dynamic_cast<EBStrokeItem *>(item))
            snapshot.append(stroke->state());
    }
    return snapshot;
}

void EBBoardScene::restoreStrokes(const StrokeSnapshot &snapshot)
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

EBPage::Texts EBBoardScene::captureTexts() const
{
    EBPage::Texts texts;
    for (QGraphicsItem *item : items(Qt::AscendingOrder)) {
        if (auto *text = dynamic_cast<EBTextItem *>(item))
            texts.append(text->state());
    }
    return texts;
}

void EBBoardScene::restoreTexts(const EBPage::Texts &texts)
{
    for (QGraphicsItem *item : items()) {
        if (auto *text = dynamic_cast<EBTextItem *>(item))
            delete text;
    }
    for (const EBTextItem::State &state : texts) {
        EBTextItem *text = addText(state.text, state.font, state.color);
        text->applyState(state);
    }
}

EBPage::Images EBBoardScene::captureImages() const
{
    EBPage::Images images;
    for (QGraphicsItem *item : items(Qt::AscendingOrder)) {
        if (auto *image = dynamic_cast<EBImageItem *>(item))
            images.append(image->state());
    }
    return images;
}

void EBBoardScene::restoreImages(const EBPage::Images &images)
{
    for (QGraphicsItem *item : items()) {
        if (auto *image = dynamic_cast<EBImageItem *>(item))
            delete image;
    }
    for (const EBImageItem::State &state : images)
        addImage(state);
}

EBBoardScene::Snapshot EBBoardScene::captureSnapshot() const
{
    return {captureStrokes(), captureTexts(), captureImages()};
}

void EBBoardScene::restoreSnapshot(const Snapshot &snapshot)
{
    restoreStrokes(snapshot.strokes);
    restoreTexts(snapshot.texts);
    restoreImages(snapshot.images);
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

EBStrokeItem *EBBoardScene::strokeAt(const QPointF &scenePosition,
                                     qreal tolerance) const
{
    QPainterPath hitArea;
    hitArea.addEllipse(scenePosition, tolerance, tolerance);
    for (QGraphicsItem *item : items(hitArea, Qt::IntersectsItemShape,
                                     Qt::DescendingOrder)) {
        if (auto *stroke = dynamic_cast<EBStrokeItem *>(item))
            return stroke;
    }
    return nullptr;
}

EBTextItem *EBBoardScene::textAt(const QPointF &scenePosition) const
{
    for (QGraphicsItem *item : items(scenePosition, Qt::IntersectsItemShape,
                                     Qt::DescendingOrder)) {
        if (auto *text = dynamic_cast<EBTextItem *>(item))
            return text;
    }
    return nullptr;
}

QGraphicsItem *EBBoardScene::objectAt(const QPointF &scenePosition,
                                      qreal tolerance) const
{
    QPainterPath hitArea;
    hitArea.addEllipse(scenePosition, tolerance, tolerance);
    for (QGraphicsItem *item : items(hitArea, Qt::IntersectsItemShape,
                                     Qt::DescendingOrder)) {
        if (dynamic_cast<EBStrokeItem *>(item)
            || dynamic_cast<EBTextItem *>(item)
            || dynamic_cast<EBImageItem *>(item))
            return item;
    }
    return nullptr;
}

EBStrokeItem *EBBoardScene::selectedStroke() const
{
    for (QGraphicsItem *item : selectedItems()) {
        if (auto *stroke = dynamic_cast<EBStrokeItem *>(item))
            return stroke;
    }
    return nullptr;
}

QGraphicsItem *EBBoardScene::selectedObject() const
{
    for (QGraphicsItem *item : selectedItems()) {
        if (dynamic_cast<EBStrokeItem *>(item)
            || dynamic_cast<EBTextItem *>(item)
            || dynamic_cast<EBImageItem *>(item))
            return item;
    }
    return nullptr;
}

void EBBoardScene::setObjectInteractionEnabled(bool enabled)
{
    if (_objectInteractionEnabled == enabled)
        return;
    _objectInteractionEnabled = enabled;
    for (QGraphicsItem *item : items()) {
        if (dynamic_cast<EBStrokeItem *>(item)
            || dynamic_cast<EBTextItem *>(item)
            || dynamic_cast<EBImageItem *>(item))
            item->setFlag(QGraphicsItem::ItemIsSelectable, enabled);
    }
    if (!enabled)
        clearSelection();
}

bool EBBoardScene::objectInteractionEnabled() const
{
    return _objectInteractionEnabled;
}

void EBBoardScene::refreshPageGeometry()
{
    const qreal width = _pageSize == PageSize::Standard
        ? kStandardPageWidth : kWidescreenPageWidth;
    _pageRect = QRectF(kPageMargin, kPageMargin, width, kPageHeight);
    _pageItem->setRect(_pageRect);
    setSceneRect(0.0, 0.0, width + 2.0 * kPageMargin,
                 kPageHeight + 2.0 * kPageMargin);
    refreshPageImage();
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

void EBBoardScene::refreshPageImage()
{
    if (!_pageImageItem || _pageBackgroundImage.isNull()) {
        if (_pageImageItem)
            _pageImageItem->hide();
        return;
    }

    const QImage scaled = _pageBackgroundImage.scaled(
        _pageRect.size().toSize(), Qt::KeepAspectRatio,
        Qt::SmoothTransformation);
    _pageImageItem->setPixmap(QPixmap::fromImage(scaled));
    _pageImageItem->setPos(
        _pageRect.left() + (_pageRect.width() - scaled.width()) / 2.0,
        _pageRect.top() + (_pageRect.height() - scaled.height()) / 2.0);
    _pageImageItem->show();
}
