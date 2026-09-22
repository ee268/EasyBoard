#include "ebobjectclipboard.h"

#include <QBuffer>
#include <QDataStream>
#include <QGraphicsItem>
#include <QImage>
#include <QMimeData>
#include <QtMath>

namespace {
constexpr quint32 kClipboardMagic = 0x45424f42; // EBOB
constexpr quint16 kClipboardVersion = 1;
constexpr int kMaximumPayloadSize = 70 * 1024 * 1024;

bool finite(qreal value)
{
    return qIsFinite(value);
}

bool validTransform(const QPointF &position, qreal zValue,
                    const QPointF &origin, qreal scale, qreal rotation)
{
    return finite(position.x()) && finite(position.y()) && finite(zValue)
        && finite(origin.x()) && finite(origin.y()) && finite(scale)
        && finite(rotation) && scale > 0.0 && scale <= 100.0;
}
}

QString EBObjectClipboard::mimeType()
{
    return QStringLiteral("application/x-easyboard-object");
}

QMimeData *EBObjectClipboard::createMimeData(QGraphicsItem *item)
{
    Object object;
    if (auto *stroke = dynamic_cast<EBStrokeItem *>(item)) {
        object.type = Type::Stroke;
        object.stroke = stroke->state();
    } else if (auto *text = dynamic_cast<EBTextItem *>(item)) {
        object.type = Type::Text;
        object.text = text->state();
    } else if (auto *image = dynamic_cast<EBImageItem *>(item)) {
        object.type = Type::Image;
        object.image = image->state();
    } else {
        return nullptr;
    }

    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_5_15);
    stream << kClipboardMagic << kClipboardVersion
           << static_cast<quint8>(object.type);
    if (object.type == Type::Stroke) {
        const auto &state = object.stroke;
        stream << state.path << state.pen << state.position << state.zValue
               << state.transformOrigin << state.scale << state.rotation;
    } else if (object.type == Type::Text) {
        const auto &state = object.text;
        stream << state.text << state.font << state.color << state.position
               << state.zValue << state.transformOrigin << state.scale
               << state.rotation << state.textWidth;
    } else {
        const auto &state = object.image;
        stream << static_cast<quint8>(state.format) << state.data << state.size
               << state.position << state.zValue << state.transformOrigin
               << state.scale << state.rotation;
    }
    if (stream.status() != QDataStream::Ok
        || payload.size() > kMaximumPayloadSize)
        return nullptr;

    QMimeData *mimeData = new QMimeData;
    mimeData->setData(mimeType(), payload);
    if (object.type == Type::Text)
        mimeData->setText(object.text.text);
    if (object.type == Type::Image) {
        if (object.image.format == EBImageItem::Format::Svg) {
            mimeData->setData(QStringLiteral("image/svg+xml"),
                              object.image.data);
        } else {
            QImage image;
            if (image.loadFromData(object.image.data, "PNG"))
                mimeData->setImageData(image);
        }
    }
    return mimeData;
}

bool EBObjectClipboard::decode(const QMimeData *mimeData, Object *object)
{
    if (!mimeData || !object || !mimeData->hasFormat(mimeType()))
        return false;
    const QByteArray payload = mimeData->data(mimeType());
    if (payload.isEmpty() || payload.size() > kMaximumPayloadSize)
        return false;

    QDataStream stream(payload);
    stream.setVersion(QDataStream::Qt_5_15);
    quint32 magic = 0;
    quint16 version = 0;
    quint8 rawType = 0;
    stream >> magic >> version >> rawType;
    if (magic != kClipboardMagic || version != kClipboardVersion)
        return false;

    Object decoded;
    decoded.type = static_cast<Type>(rawType);
    if (decoded.type == Type::Stroke) {
        auto &state = decoded.stroke;
        stream >> state.path >> state.pen >> state.position >> state.zValue
               >> state.transformOrigin >> state.scale >> state.rotation;
        if (state.path.isEmpty() || !state.pen.color().isValid()
            || !validTransform(state.position, state.zValue,
                               state.transformOrigin, state.scale,
                               state.rotation))
            return false;
    } else if (decoded.type == Type::Text) {
        auto &state = decoded.text;
        stream >> state.text >> state.font >> state.color >> state.position
               >> state.zValue >> state.transformOrigin >> state.scale
               >> state.rotation >> state.textWidth;
        if (state.text.isEmpty() || state.text.size() > 100000
            || !state.color.isValid() || !finite(state.textWidth)
            || state.textWidth <= 0.0 || state.textWidth > 100000.0
            || !validTransform(state.position, state.zValue,
                               state.transformOrigin, state.scale,
                               state.rotation))
            return false;
    } else if (decoded.type == Type::Image) {
        auto &state = decoded.image;
        quint8 rawFormat = 0;
        stream >> rawFormat >> state.data >> state.size >> state.position
               >> state.zValue >> state.transformOrigin >> state.scale
               >> state.rotation;
        if (rawFormat > static_cast<quint8>(EBImageItem::Format::Svg))
            return false;
        state.format = static_cast<EBImageItem::Format>(rawFormat);
        QSizeF naturalSize;
        if (!EBImageItem::naturalSize(state.format, state.data, &naturalSize)
            || !finite(state.size.width()) || !finite(state.size.height())
            || !state.size.isValid() || state.size.isEmpty()
            || !validTransform(state.position, state.zValue,
                               state.transformOrigin, state.scale,
                               state.rotation))
            return false;
    } else {
        return false;
    }
    if (stream.status() != QDataStream::Ok || !stream.atEnd())
        return false;
    *object = decoded;
    return true;
}

bool EBObjectClipboard::canDecode(const QMimeData *mimeData)
{
    Object object;
    return decode(mimeData, &object);
}
