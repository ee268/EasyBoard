#ifndef EBOBJECTCLIPBOARD_H
#define EBOBJECTCLIPBOARD_H

#include "ebimageitem.h"
#include "ebstrokeitem.h"
#include "ebtextitem.h"

class QGraphicsItem;
class QMimeData;

// EasyBoard 对象剪贴板保留对象的内容、层级和变换状态。
class EBObjectClipboard
{
public:
    enum class Type {
        Invalid,
        Stroke,
        Text,
        Image
    };

    struct Object {
        Type type = Type::Invalid;
        EBStrokeItem::State stroke;
        EBTextItem::State text;
        EBImageItem::State image;
    };

    static QString mimeType();
    static QMimeData *createMimeData(QGraphicsItem *item);
    static bool decode(const QMimeData *mimeData, Object *object);
    static bool canDecode(const QMimeData *mimeData);
};

#endif
