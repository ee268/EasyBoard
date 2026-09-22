#ifndef EBTEXTITEM_H
#define EBTEXTITEM_H

#include <QColor>
#include <QFont>
#include <QGraphicsTextItem>

// 可编辑文字对象；内容和几何状态与笔迹一样由页面模型持久化。
class EBTextItem : public QGraphicsTextItem
{
public:
    struct State {
        QString text;
        QFont font;
        QColor color;
        QPointF position;
        qreal zValue;
        QPointF transformOrigin;
        qreal scale = 1.0;
        qreal rotation = 0.0;
        qreal textWidth = 360.0;
    };

    EBTextItem(const QString &text, const QFont &font, const QColor &color);

    State state() const;
    void applyState(const State &state);
    void refreshTransformOrigin();
};

#endif
