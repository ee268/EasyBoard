#ifndef EBTEACHINGSTATE_H
#define EBTEACHINGSTATE_H

#include <QPointF>
#include <QRectF>

// 坐标相对页面左上角；教具状态随页面保存，绘制笔迹仍由场景管理。
struct EBTeachingState
{
    enum class Kind {
        None, Ruler, Triangle45, Triangle30, Protractor,
        Compass, Curtain, Spotlight, Magnifier
    };

    Kind kind = Kind::None;
    QPointF center;
    QRectF frame;
    qreal rotation = 0.0;
    qreal scale = 1.0;
    qreal radius = 100.0;
    qreal measuredAngle = 0.0;
    qreal magnification = 2.0;
    bool flipHorizontal = false;
    bool flipVertical = false;
    bool rectangularLens = false;
};

#endif
