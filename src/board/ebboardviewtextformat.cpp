#include "ebboardview.h"

#include "ebtextitem.h"

bool EBBoardView::canFormatSelectedText() const
{
    if (_editingText || !_scene->selectedObjectsEditable())
        return false;
    const QVector<QGraphicsItem *> objects = _scene->selectedObjects();
    if (objects.isEmpty())
        return false;
    for (QGraphicsItem *object : objects) {
        if (!dynamic_cast<EBTextItem *>(object))
            return false;
    }
    return true;
}

QFont EBBoardView::selectedTextFont() const
{
    for (QGraphicsItem *object : _scene->selectedObjects()) {
        if (auto *text = dynamic_cast<EBTextItem *>(object))
            return text->font();
    }
    return QFont();
}

QColor EBBoardView::selectedTextColor() const
{
    for (QGraphicsItem *object : _scene->selectedObjects()) {
        if (auto *text = dynamic_cast<EBTextItem *>(object))
            return text->defaultTextColor();
    }
    return QColor();
}

void EBBoardView::setSelectedTextFont(const QFont &font)
{
    if (!canFormatSelectedText() || font.pointSizeF() < 4.0
        || font.pointSizeF() > 144.0)
        return;
    const Snapshot before = _scene->captureSnapshot();
    const QVector<QGraphicsItem *> objects = _scene->selectedObjects();
    bool changed = false;
    for (QGraphicsItem *object : objects) {
        EBTextItem *text = static_cast<EBTextItem *>(object);
        if (text->font() == font)
            continue;
        text->setFont(font);
        text->refreshTransformOrigin();
        changed = true;
    }
    if (!changed)
        return;
    keepObjectsInsidePage(objects);
    commitObjectEdit(before, tr("修改文字字体"));
}

void EBBoardView::setSelectedTextColor(const QColor &color)
{
    if (!canFormatSelectedText() || !color.isValid())
        return;
    const Snapshot before = _scene->captureSnapshot();
    bool changed = false;
    for (QGraphicsItem *object : _scene->selectedObjects()) {
        EBTextItem *text = static_cast<EBTextItem *>(object);
        if (text->defaultTextColor() == color)
            continue;
        text->setDefaultTextColor(color);
        changed = true;
    }
    if (changed)
        commitObjectEdit(before, tr("修改文字颜色"));
}
