#ifndef EBSTROKEITEM_H
#define EBSTROKEITEM_H

#include <QGraphicsPathItem>
#include <QPen>
#include <QVector>

// 一条可编辑笔迹；页面场景负责创建和管理，图元只处理自身几何。
class EBStrokeItem : public QGraphicsPathItem
{
public:
    struct State {
        QPainterPath path;
        QPen pen;
        QPointF position;
        qreal zValue;
        QPointF transformOrigin;
        qreal scale = 1.0;
        qreal rotation = 0.0;
        QString groupId;
        bool locked = false;
    };

    EBStrokeItem(const QPainterPath &path, const QPen &pen);
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget = nullptr) override;

    // 保存笔迹可恢复的状态，供撤销命令构造快照。
    State state() const;
    void applyState(const State &state);
    QString groupId() const;
    void setGroupId(const QString &groupId);
    bool isLocked() const;
    void setLocked(bool locked);

    // 用圆形橡皮裁切本笔迹；返回 false 表示没有擦到实际路径。
    bool splitAt(const QPointF &center, qreal radius,
                 QVector<QPainterPath> &remaining) const;

private:
    QString _groupId;
    bool _locked = false;
};

#endif
