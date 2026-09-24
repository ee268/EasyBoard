#ifndef EBSCREENCAPTURE_H
#define EBSCREENCAPTURE_H

#include <QImage>
#include <QRect>

// 按逻辑屏幕坐标截图，并将不同缩放比例的屏幕合成一张图片。
QImage ebCaptureScreens(const QRect &area, qreal devicePixelRatio);

#endif
