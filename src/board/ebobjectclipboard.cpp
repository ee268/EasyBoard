#include "ebobjectclipboard.h"

#include <QDataStream>
#include <QGraphicsItem>
#include <QHash>
#include <QImage>
#include <QMimeData>
#include <QStringList>
#include <QUuid>
#include <QtMath>

namespace {
constexpr quint32 kClipboardMagic = 0x45424f42; // EBOB
constexpr quint16 kClipboardVersion = 3;
constexpr int kMaximumObjectCount = 1000;
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

bool validGroupId(const QString &groupId)
{
    return groupId.isEmpty() || !QUuid(groupId).isNull();
}

QString &objectGroupId(EBObjectClipboard::Object &object)
{
    if (object.type == EBObjectClipboard::Type::Stroke)
        return object.stroke.groupId;
    if (object.type == EBObjectClipboard::Type::Text)
        return object.text.groupId;
    return object.image.groupId;
}

EBObjectClipboard::Object objectFromItem(QGraphicsItem *item)
{
    EBObjectClipboard::Object object;
    if (auto *stroke = dynamic_cast<EBStrokeItem *>(item)) {
        object.type = EBObjectClipboard::Type::Stroke;
        object.stroke = stroke->state();
    } else if (auto *text = dynamic_cast<EBTextItem *>(item)) {
        object.type = EBObjectClipboard::Type::Text;
        object.text = text->state();
    } else if (auto *image = dynamic_cast<EBImageItem *>(item)) {
        object.type = EBObjectClipboard::Type::Image;
        object.image = image->state();
    }
    return object;
}

void writeObject(QDataStream &stream, const EBObjectClipboard::Object &object)
{
    stream << static_cast<quint8>(object.type);
    if (object.type == EBObjectClipboard::Type::Stroke) {
        const auto &state = object.stroke;
        stream << state.path << state.pen << state.position << state.zValue
               << state.transformOrigin << state.scale << state.rotation
               << state.groupId;
    } else if (object.type == EBObjectClipboard::Type::Text) {
        const auto &state = object.text;
        stream << state.text << state.font << state.color << state.position
               << state.zValue << state.transformOrigin << state.scale
               << state.rotation << state.textWidth << state.groupId;
    } else {
        const auto &state = object.image;
        stream << static_cast<quint8>(state.format) << state.data << state.size
               << state.position << state.zValue << state.transformOrigin
               << state.scale << state.rotation << state.groupId;
    }
}

bool readObject(QDataStream &stream, quint16 version,
                EBObjectClipboard::Object *object)
{
    quint8 rawType = 0;
    stream >> rawType;
    object->type = static_cast<EBObjectClipboard::Type>(rawType);
    if (object->type == EBObjectClipboard::Type::Stroke) {
        auto &state = object->stroke;
        stream >> state.path >> state.pen >> state.position >> state.zValue
               >> state.transformOrigin >> state.scale >> state.rotation;
        if (version >= 3)
            stream >> state.groupId;
        return !state.path.isEmpty() && state.pen.color().isValid()
            && validGroupId(state.groupId)
            && validTransform(state.position, state.zValue,
                              state.transformOrigin, state.scale,
                              state.rotation);
    }
    if (object->type == EBObjectClipboard::Type::Text) {
        auto &state = object->text;
        stream >> state.text >> state.font >> state.color >> state.position
               >> state.zValue >> state.transformOrigin >> state.scale
               >> state.rotation >> state.textWidth;
        if (version >= 3)
            stream >> state.groupId;
        return !state.text.isEmpty() && state.text.size() <= 100000
            && validGroupId(state.groupId)
            && state.color.isValid() && finite(state.textWidth)
            && state.textWidth > 0.0 && state.textWidth <= 100000.0
            && validTransform(state.position, state.zValue,
                              state.transformOrigin, state.scale,
                              state.rotation);
    }
    if (object->type == EBObjectClipboard::Type::Image) {
        auto &state = object->image;
        quint8 rawFormat = 0;
        stream >> rawFormat >> state.data >> state.size >> state.position
               >> state.zValue >> state.transformOrigin >> state.scale
               >> state.rotation;
        if (version >= 3)
            stream >> state.groupId;
        if (rawFormat > static_cast<quint8>(EBImageItem::Format::Svg))
            return false;
        state.format = static_cast<EBImageItem::Format>(rawFormat);
        return EBImageItem::naturalSize(state.format, state.data)
            && validGroupId(state.groupId)
            && finite(state.size.width()) && finite(state.size.height())
            && state.size.isValid() && !state.size.isEmpty()
            && validTransform(state.position, state.zValue,
                              state.transformOrigin, state.scale,
                              state.rotation);
    }
    return false;
}
}

QString EBObjectClipboard::mimeType()
{
    return QStringLiteral("application/x-easyboard-object");
}

QMimeData *EBObjectClipboard::createMimeData(
    const QVector<QGraphicsItem *> &items)
{
    if (items.isEmpty() || items.size() > kMaximumObjectCount)
        return nullptr;
    Objects objects;
    QStringList plainTexts;
    for (QGraphicsItem *item : items) {
        const Object object = objectFromItem(item);
        if (object.type == Type::Invalid)
            return nullptr;
        objects.append(object);
        if (object.type == Type::Text)
            plainTexts.append(object.text.text);
    }

    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_5_15);
    stream << kClipboardMagic << kClipboardVersion
           << static_cast<quint32>(objects.size());
    for (const Object &object : objects)
        writeObject(stream, object);
    if (stream.status() != QDataStream::Ok
        || payload.size() > kMaximumPayloadSize)
        return nullptr;

    QMimeData *mimeData = new QMimeData;
    mimeData->setData(mimeType(), payload);
    if (!plainTexts.isEmpty())
        mimeData->setText(plainTexts.join(QLatin1Char('\n')));
    if (objects.size() == 1 && objects.first().type == Type::Image) {
        const EBImageItem::State &image = objects.first().image;
        if (image.format == EBImageItem::Format::Svg)
            mimeData->setData(QStringLiteral("image/svg+xml"), image.data);
        else
            mimeData->setImageData(QImage::fromData(image.data, "PNG"));
    }
    return mimeData;
}

bool EBObjectClipboard::decode(const QMimeData *mimeData, Objects *objects)
{
    if (!mimeData || !objects || !mimeData->hasFormat(mimeType()))
        return false;
    const QByteArray payload = mimeData->data(mimeType());
    if (payload.isEmpty() || payload.size() > kMaximumPayloadSize)
        return false;

    QDataStream stream(payload);
    stream.setVersion(QDataStream::Qt_5_15);
    quint32 magic = 0;
    quint16 version = 0;
    quint32 count = 0;
    stream >> magic >> version >> count;
    if (magic != kClipboardMagic || (version != 2 && version != kClipboardVersion)
        || count == 0 || count > kMaximumObjectCount)
        return false;

    Objects decoded;
    decoded.reserve(static_cast<int>(count));
    for (quint32 index = 0; index < count; ++index) {
        Object object;
        if (!readObject(stream, version, &object))
            return false;
        decoded.append(object);
    }
    if (stream.status() != QDataStream::Ok || !stream.atEnd())
        return false;
    *objects = decoded;
    return true;
}

bool EBObjectClipboard::canDecode(const QMimeData *mimeData)
{
    Objects objects;
    return decode(mimeData, &objects);
}

void EBObjectClipboard::remapGroupIds(Objects *objects)
{
    if (!objects)
        return;
    QHash<QString, QString> replacements;
    for (Object &object : *objects) {
        QString &groupId = objectGroupId(object);
        if (groupId.isEmpty())
            continue;
        if (!replacements.contains(groupId)) {
            replacements.insert(groupId,
                QUuid::createUuid().toString(QUuid::WithoutBraces));
        }
        groupId = replacements.value(groupId);
    }
}
