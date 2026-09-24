#ifndef EBDESKTOPINKSTORE_H
#define EBDESKTOPINKSTORE_H

#include <QColor>
#include <QHash>
#include <QPainterPath>
#include <QSize>
#include <QVector>

class QScreen;

class EBDesktopInkStore
{
public:
    enum class Tool { Pen, Marker, Eraser, Line, Rectangle, Ellipse };
    struct Stroke {
        Tool tool;
        QPainterPath path;
        QColor color;
        qreal width;
    };
    struct ScreenInk {
        QVector<Stroke> strokes;
        int applied = 0;
        QSize size;
    };

    static QString screenId(const QScreen *screen);
    static QHash<QString, ScreenInk> load();
    static bool save(const QHash<QString, ScreenInk> &screens);
};

#endif
