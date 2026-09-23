#include "ebdocumentstorage.h"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QUuid>
#include <QtMath>

#include <algorithm>

#include "../core/ebsettings.h"
#include "../domain/ebdocument.h"

namespace {
constexpr qint64 kMaxBackgroundPixels = 40000000;
constexpr int kMaxBackgroundDataBytes = 64 * 1024 * 1024;
constexpr int kMaxTextLength = 10000;
constexpr int kMaxObjectImageDataBytes = 64 * 1024 * 1024;
constexpr int kMaxGuidesPerAxis = 100;

QString colorName(EBPage::Color color)
{
    return color == EBPage::Color::White ? QStringLiteral("white")
                                         : QStringLiteral("cream");
}

QString patternName(EBPage::Pattern pattern)
{
    if (pattern == EBPage::Pattern::Grid)
        return QStringLiteral("grid");
    if (pattern == EBPage::Pattern::Ruled)
        return QStringLiteral("ruled");
    return QStringLiteral("blank");
}

QString sizeName(EBPage::Size size)
{
    return size == EBPage::Size::Standard ? QStringLiteral("standard")
                                          : QStringLiteral("widescreen");
}

QJsonObject pathObject(const QPainterPath &path)
{
    QJsonArray elements;
    for (int index = 0; index < path.elementCount(); ++index) {
        const QPainterPath::Element element = path.elementAt(index);
        elements.append(QJsonObject{
            {QStringLiteral("type"), int(element.type)},
            {QStringLiteral("x"), element.x},
            {QStringLiteral("y"), element.y}
        });
    }
    return QJsonObject{{QStringLiteral("fillRule"), int(path.fillRule())},
                       {QStringLiteral("elements"), elements}};
}

QJsonObject penObject(const QPen &pen)
{
    return QJsonObject{
        {QStringLiteral("color"), pen.color().name(QColor::HexArgb)},
        {QStringLiteral("width"), pen.widthF()},
        {QStringLiteral("style"), int(pen.style())},
        {QStringLiteral("cap"), int(pen.capStyle())},
        {QStringLiteral("join"), int(pen.joinStyle())},
        {QStringLiteral("cosmetic"), pen.isCosmetic()}
    };
}

QJsonObject strokeObject(const EBStrokeItem::State &stroke)
{
    QJsonObject object{
        {QStringLiteral("path"), pathObject(stroke.path)},
        {QStringLiteral("pen"), penObject(stroke.pen)},
        {QStringLiteral("position"), QJsonObject{
             {QStringLiteral("x"), stroke.position.x()},
             {QStringLiteral("y"), stroke.position.y()}}},
        {QStringLiteral("z"), stroke.zValue},
        {QStringLiteral("transformOrigin"), QJsonObject{
             {QStringLiteral("x"), stroke.transformOrigin.x()},
             {QStringLiteral("y"), stroke.transformOrigin.y()}}},
        {QStringLiteral("scale"), stroke.scale},
        {QStringLiteral("rotation"), stroke.rotation}
    };
    if (!stroke.groupId.isEmpty())
        object.insert(QStringLiteral("groupId"), stroke.groupId);
    if (stroke.locked)
        object.insert(QStringLiteral("locked"), true);
    return object;
}

QJsonObject fontObject(const QFont &font)
{
    return QJsonObject{
        {QStringLiteral("family"), font.family()},
        {QStringLiteral("pointSize"), font.pointSizeF()},
        {QStringLiteral("weight"), font.weight()},
        {QStringLiteral("italic"), font.italic()},
        {QStringLiteral("underline"), font.underline()}
    };
}

QJsonObject textObject(const EBTextItem::State &text)
{
    QJsonObject object{
        {QStringLiteral("text"), text.text},
        {QStringLiteral("font"), fontObject(text.font)},
        {QStringLiteral("color"), text.color.name(QColor::HexArgb)},
        {QStringLiteral("position"), QJsonObject{
             {QStringLiteral("x"), text.position.x()},
             {QStringLiteral("y"), text.position.y()}}},
        {QStringLiteral("z"), text.zValue},
        {QStringLiteral("transformOrigin"), QJsonObject{
             {QStringLiteral("x"), text.transformOrigin.x()},
             {QStringLiteral("y"), text.transformOrigin.y()}}},
        {QStringLiteral("scale"), text.scale},
        {QStringLiteral("rotation"), text.rotation},
        {QStringLiteral("textWidth"), text.textWidth}
    };
    if (!text.groupId.isEmpty())
        object.insert(QStringLiteral("groupId"), text.groupId);
    if (text.locked)
        object.insert(QStringLiteral("locked"), true);
    return object;
}

QJsonObject imageObject(const EBImageItem::State &image)
{
    QJsonObject object{
        {QStringLiteral("format"), image.format == EBImageItem::Format::Svg
             ? QStringLiteral("svg") : QStringLiteral("png")},
        {QStringLiteral("data"), QString::fromLatin1(image.data.toBase64())},
        {QStringLiteral("size"), QJsonObject{
             {QStringLiteral("width"), image.size.width()},
             {QStringLiteral("height"), image.size.height()}}},
        {QStringLiteral("position"), QJsonObject{
             {QStringLiteral("x"), image.position.x()},
             {QStringLiteral("y"), image.position.y()}}},
        {QStringLiteral("z"), image.zValue},
        {QStringLiteral("transformOrigin"), QJsonObject{
             {QStringLiteral("x"), image.transformOrigin.x()},
             {QStringLiteral("y"), image.transformOrigin.y()}}},
        {QStringLiteral("scale"), image.scale},
        {QStringLiteral("rotation"), image.rotation}
    };
    if (!image.groupId.isEmpty())
        object.insert(QStringLiteral("groupId"), image.groupId);
    if (image.locked)
        object.insert(QStringLiteral("locked"), true);
    return object;
}

QJsonObject pageObject(const EBPage &page, int index)
{
    QJsonArray strokes;
    for (const EBStrokeItem::State &stroke : page.strokes())
        strokes.append(strokeObject(stroke));
    QJsonArray texts;
    for (const EBTextItem::State &text : page.texts())
        texts.append(textObject(text));
    QJsonArray images;
    for (const EBImageItem::State &image : page.images())
        images.append(imageObject(image));
    QJsonArray horizontalGuides;
    for (qreal guide : page.horizontalGuides())
        horizontalGuides.append(guide);
    QJsonArray verticalGuides;
    for (qreal guide : page.verticalGuides())
        verticalGuides.append(guide);
    QJsonObject result{
        {QStringLiteral("index"), index},
        {QStringLiteral("color"), colorName(page.color())},
        {QStringLiteral("pattern"), patternName(page.pattern())},
        {QStringLiteral("size"), sizeName(page.size())},
        {QStringLiteral("strokes"), strokes},
        {QStringLiteral("texts"), texts},
        {QStringLiteral("images"), images}
    };
    if (!horizontalGuides.isEmpty() || !verticalGuides.isEmpty()) {
        result.insert(QStringLiteral("guides"), QJsonObject{
            {QStringLiteral("horizontal"), horizontalGuides},
            {QStringLiteral("vertical"), verticalGuides}
        });
    }
    if (page.hasBackgroundImage()) {
        QByteArray png;
        QBuffer buffer(&png);
        buffer.open(QIODevice::WriteOnly);
        page.backgroundImage().save(&buffer, "PNG");
        result.insert(QStringLiteral("backgroundImage"), QJsonObject{
            {QStringLiteral("format"), QStringLiteral("png")},
            {QStringLiteral("data"), QString::fromLatin1(png.toBase64())}
        });
    }
    return result;
}

bool readBackgroundImage(const QJsonObject &object, QImage *image)
{
    if (!object.contains(QStringLiteral("backgroundImage"))) {
        *image = QImage();
        return true;
    }

    const QJsonObject stored = object.value(
        QStringLiteral("backgroundImage")).toObject();
    const QString encoded = stored.value(QStringLiteral("data")).toString();
    if (stored.value(QStringLiteral("format")).toString()
            != QStringLiteral("png")
        || encoded.isEmpty()
        || encoded.size() > kMaxBackgroundDataBytes * 2)
        return false;

    const QByteArray data = QByteArray::fromBase64(encoded.toLatin1());
    if (data.isEmpty() || data.size() > kMaxBackgroundDataBytes)
        return false;
    QBuffer buffer;
    buffer.setData(data);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer, "PNG");
    const QSize size = reader.size();
    if (!size.isValid()
        || qint64(size.width()) * qint64(size.height()) > kMaxBackgroundPixels)
        return false;
    const QImage decoded = reader.read();
    if (decoded.isNull())
        return false;
    *image = decoded;
    return true;
}

bool readPath(const QJsonObject &object, QPainterPath *path)
{
    const QJsonArray elements = object.value(QStringLiteral("elements")).toArray();
    const int fillRule = object.value(QStringLiteral("fillRule")).toInt(-1);
    if (elements.isEmpty() || fillRule < int(Qt::OddEvenFill)
        || fillRule > int(Qt::WindingFill))
        return false;

    QPainterPath result;
    for (int index = 0; index < elements.size(); ++index) {
        const QJsonObject element = elements.at(index).toObject();
        if (!element.contains(QStringLiteral("x"))
            || !element.contains(QStringLiteral("y")))
            return false;
        const qreal x = element.value(QStringLiteral("x")).toDouble();
        const qreal y = element.value(QStringLiteral("y")).toDouble();
        const int type = element.value(QStringLiteral("type")).toInt(-1);
        if (index == 0 && type != int(QPainterPath::MoveToElement))
            return false;
        if (type == int(QPainterPath::MoveToElement)) {
            result.moveTo(x, y);
        } else if (type == int(QPainterPath::LineToElement)) {
            result.lineTo(x, y);
        } else if (type == int(QPainterPath::CurveToElement)) {
            if (index + 2 >= elements.size())
                return false;
            const QJsonObject control = elements.at(index + 1).toObject();
            const QJsonObject end = elements.at(index + 2).toObject();
            if (!control.contains(QStringLiteral("x"))
                || !control.contains(QStringLiteral("y"))
                || !end.contains(QStringLiteral("x"))
                || !end.contains(QStringLiteral("y"))
                || control.value(QStringLiteral("type")).toInt(-1)
                    != int(QPainterPath::CurveToDataElement)
                || end.value(QStringLiteral("type")).toInt(-1)
                    != int(QPainterPath::CurveToDataElement))
                return false;
            result.cubicTo(x, y,
                           control.value(QStringLiteral("x")).toDouble(),
                           control.value(QStringLiteral("y")).toDouble(),
                           end.value(QStringLiteral("x")).toDouble(),
                           end.value(QStringLiteral("y")).toDouble());
            index += 2;
        } else {
            return false;
        }
    }
    result.setFillRule(static_cast<Qt::FillRule>(fillRule));
    *path = result;
    return true;
}

bool readPen(const QJsonObject &object, QPen *pen)
{
    const QColor color(object.value(QStringLiteral("color")).toString());
    const qreal width = object.value(QStringLiteral("width")).toDouble(-1.0);
    const int style = object.value(QStringLiteral("style")).toInt(-1);
    const int cap = object.value(QStringLiteral("cap")).toInt(-1);
    const int join = object.value(QStringLiteral("join")).toInt(-1);
    if (!color.isValid() || width < 0.0 || style < int(Qt::NoPen)
        || style > int(Qt::CustomDashLine)
        || (cap != int(Qt::FlatCap) && cap != int(Qt::SquareCap)
            && cap != int(Qt::RoundCap))
        || (join != int(Qt::MiterJoin) && join != int(Qt::BevelJoin)
            && join != int(Qt::RoundJoin) && join != int(Qt::SvgMiterJoin)))
        return false;

    QPen result(color, width, static_cast<Qt::PenStyle>(style),
                static_cast<Qt::PenCapStyle>(cap),
                static_cast<Qt::PenJoinStyle>(join));
    result.setCosmetic(object.value(QStringLiteral("cosmetic")).toBool(false));
    *pen = result;
    return true;
}

bool readGroupId(const QJsonObject &object, QString *groupId)
{
    const QJsonValue value = object.value(QStringLiteral("groupId"));
    if (value.isUndefined()) {
        groupId->clear();
        return true;
    }
    if (!value.isString())
        return false;
    const QString id = value.toString();
    if (id.isEmpty() || QUuid(id).isNull())
        return false;
    *groupId = QUuid(id).toString(QUuid::WithoutBraces);
    return true;
}

bool readLocked(const QJsonObject &object, bool *locked)
{
    const QJsonValue value = object.value(QStringLiteral("locked"));
    if (value.isUndefined()) {
        *locked = false;
        return true;
    }
    if (!value.isBool())
        return false;
    *locked = value.toBool();
    return true;
}

bool readStroke(const QJsonObject &object, EBStrokeItem::State *stroke)
{
    QPainterPath path;
    QPen pen;
    const QJsonObject position = object.value(QStringLiteral("position")).toObject();
    if (!readPath(object.value(QStringLiteral("path")).toObject(), &path)
        || !readPen(object.value(QStringLiteral("pen")).toObject(), &pen)
        || !position.contains(QStringLiteral("x"))
        || !position.contains(QStringLiteral("y"))
        || !object.contains(QStringLiteral("z")))
        return false;
    QPointF transformOrigin = path.boundingRect().center();
    if (object.contains(QStringLiteral("transformOrigin"))) {
        const QJsonObject origin = object.value(
            QStringLiteral("transformOrigin")).toObject();
        if (!origin.contains(QStringLiteral("x"))
            || !origin.contains(QStringLiteral("y")))
            return false;
        transformOrigin = QPointF(origin.value(QStringLiteral("x")).toDouble(),
                                  origin.value(QStringLiteral("y")).toDouble());
    }
    const qreal scale = object.value(QStringLiteral("scale")).toDouble(1.0);
    const qreal rotation = object.value(QStringLiteral("rotation")).toDouble(0.0);
    QString groupId;
    bool locked = false;
    if (!qIsFinite(scale) || scale <= 0.0 || scale > 20.0
        || !qIsFinite(rotation)
        || !qIsFinite(transformOrigin.x()) || !qIsFinite(transformOrigin.y())
        || !readGroupId(object, &groupId) || !readLocked(object, &locked))
        return false;
    *stroke = {path, pen,
               QPointF(position.value(QStringLiteral("x")).toDouble(),
                       position.value(QStringLiteral("y")).toDouble()),
               object.value(QStringLiteral("z")).toDouble(),
               transformOrigin, scale, rotation, groupId, locked};
    return true;
}

bool readText(const QJsonObject &object, EBTextItem::State *text)
{
    const QString content = object.value(QStringLiteral("text")).toString();
    const QJsonObject fontJson = object.value(QStringLiteral("font")).toObject();
    const QString family = fontJson.value(QStringLiteral("family")).toString();
    const qreal pointSize = fontJson.value(QStringLiteral("pointSize")).toDouble(-1.0);
    const int weight = fontJson.value(QStringLiteral("weight")).toInt(-1);
    const QColor color(object.value(QStringLiteral("color")).toString());
    const QJsonObject position = object.value(QStringLiteral("position")).toObject();
    const QJsonObject origin = object.value(QStringLiteral("transformOrigin")).toObject();
    const qreal x = position.value(QStringLiteral("x")).toDouble();
    const qreal y = position.value(QStringLiteral("y")).toDouble();
    const qreal originX = origin.value(QStringLiteral("x")).toDouble();
    const qreal originY = origin.value(QStringLiteral("y")).toDouble();
    const qreal z = object.value(QStringLiteral("z")).toDouble();
    const qreal scale = object.value(QStringLiteral("scale")).toDouble(-1.0);
    const qreal rotation = object.value(QStringLiteral("rotation")).toDouble();
    const qreal textWidth = object.value(QStringLiteral("textWidth")).toDouble(-1.0);
    QString groupId;
    bool locked = false;
    if (content.isEmpty() || content.size() > kMaxTextLength
        || family.isEmpty() || family.size() > 256
        || pointSize < 6.0 || pointSize > 200.0
        || weight < 0 || weight > 99 || !color.isValid()
        || !position.contains(QStringLiteral("x"))
        || !position.contains(QStringLiteral("y"))
        || !origin.contains(QStringLiteral("x"))
        || !origin.contains(QStringLiteral("y"))
        || !qIsFinite(x) || !qIsFinite(y) || !qIsFinite(z)
        || !qIsFinite(originX) || !qIsFinite(originY)
        || !qIsFinite(scale) || scale <= 0.0 || scale > 20.0
        || !qIsFinite(rotation)
        || !qIsFinite(textWidth) || textWidth < 40.0 || textWidth > 2000.0
        || !readGroupId(object, &groupId) || !readLocked(object, &locked))
        return false;
    QFont font(family);
    font.setPointSizeF(pointSize);
    font.setWeight(weight);
    font.setItalic(fontJson.value(QStringLiteral("italic")).toBool(false));
    font.setUnderline(fontJson.value(QStringLiteral("underline")).toBool(false));
    *text = {content, font, color, QPointF(x, y), z,
             QPointF(originX, originY), scale, rotation, textWidth, groupId,
             locked};
    return true;
}

bool readImage(const QJsonObject &object, EBImageItem::State *image)
{
    const QString formatName = object.value(QStringLiteral("format")).toString();
    EBImageItem::Format format;
    if (formatName == QStringLiteral("png"))
        format = EBImageItem::Format::Png;
    else if (formatName == QStringLiteral("svg"))
        format = EBImageItem::Format::Svg;
    else
        return false;
    const QString encoded = object.value(QStringLiteral("data")).toString();
    if (encoded.isEmpty() || encoded.size() > kMaxObjectImageDataBytes * 2)
        return false;
    const QByteArray data = QByteArray::fromBase64(encoded.toLatin1());
    if (data.isEmpty() || data.size() > kMaxObjectImageDataBytes
        || !EBImageItem::naturalSize(format, data))
        return false;

    const QJsonObject size = object.value(QStringLiteral("size")).toObject();
    const QJsonObject position = object.value(QStringLiteral("position")).toObject();
    const QJsonObject origin = object.value(QStringLiteral("transformOrigin")).toObject();
    if (!size.contains(QStringLiteral("width"))
        || !size.contains(QStringLiteral("height"))
        || !position.contains(QStringLiteral("x"))
        || !position.contains(QStringLiteral("y"))
        || !origin.contains(QStringLiteral("x"))
        || !origin.contains(QStringLiteral("y")))
        return false;
    const qreal width = size.value(QStringLiteral("width")).toDouble();
    const qreal height = size.value(QStringLiteral("height")).toDouble();
    const qreal x = position.value(QStringLiteral("x")).toDouble();
    const qreal y = position.value(QStringLiteral("y")).toDouble();
    const qreal originX = origin.value(QStringLiteral("x")).toDouble();
    const qreal originY = origin.value(QStringLiteral("y")).toDouble();
    const qreal z = object.value(QStringLiteral("z")).toDouble();
    const qreal scale = object.value(QStringLiteral("scale")).toDouble(-1.0);
    const qreal rotation = object.value(QStringLiteral("rotation")).toDouble();
    QString groupId;
    bool locked = false;
    if (!qIsFinite(width) || !qIsFinite(height)
        || width <= 0.0 || height <= 0.0 || width > 10000.0 || height > 10000.0
        || !qIsFinite(x) || !qIsFinite(y) || !qIsFinite(z)
        || !qIsFinite(originX) || !qIsFinite(originY)
        || !qIsFinite(scale) || scale <= 0.0 || scale > 20.0
        || !qIsFinite(rotation) || !readGroupId(object, &groupId)
        || !readLocked(object, &locked))
        return false;
    *image = {format, data, QSizeF(width, height), QPointF(x, y), z,
              QPointF(originX, originY), scale, rotation, groupId, locked};
    return true;
}

bool readPage(const QJsonObject &object, int expectedIndex, EBPage *page)
{
    if (object.value(QStringLiteral("index")).toInt(-1) != expectedIndex)
        return false;
    const QString color = object.value(QStringLiteral("color")).toString();
    const QString pattern = object.value(QStringLiteral("pattern")).toString();
    const QString size = object.value(QStringLiteral("size")).toString();
    if (color == QStringLiteral("white"))
        page->setColor(EBPage::Color::White);
    else if (color == QStringLiteral("cream"))
        page->setColor(EBPage::Color::Cream);
    else
        return false;

    if (pattern == QStringLiteral("blank"))
        page->setPattern(EBPage::Pattern::Blank);
    else if (pattern == QStringLiteral("grid"))
        page->setPattern(EBPage::Pattern::Grid);
    else if (pattern == QStringLiteral("ruled"))
        page->setPattern(EBPage::Pattern::Ruled);
    else
        return false;

    if (size == QStringLiteral("standard"))
        page->setSize(EBPage::Size::Standard);
    else if (size == QStringLiteral("widescreen"))
        page->setSize(EBPage::Size::Widescreen);
    else
        return false;

    QImage backgroundImage;
    if (!readBackgroundImage(object, &backgroundImage))
        return false;
    page->setBackgroundImage(backgroundImage);

    EBPage::Strokes strokes;
    const QJsonArray strokesJson = object.value(QStringLiteral("strokes")).toArray();
    strokes.reserve(strokesJson.size());
    for (const QJsonValue &value : strokesJson) {
        EBStrokeItem::State stroke;
        if (!readStroke(value.toObject(), &stroke))
            return false;
        strokes.append(stroke);
    }
    page->setStrokes(strokes);
    EBPage::Texts texts;
    const QJsonValue textsValue = object.value(QStringLiteral("texts"));
    if (!textsValue.isUndefined() && !textsValue.isArray())
        return false;
    const QJsonArray textsJson = textsValue.toArray();
    texts.reserve(textsJson.size());
    for (const QJsonValue &value : textsJson) {
        EBTextItem::State text;
        if (!readText(value.toObject(), &text))
            return false;
        texts.append(text);
    }
    page->setTexts(texts);
    EBPage::Images images;
    const QJsonValue imagesValue = object.value(QStringLiteral("images"));
    if (!imagesValue.isUndefined() && !imagesValue.isArray())
        return false;
    const QJsonArray imagesJson = imagesValue.toArray();
    images.reserve(imagesJson.size());
    for (const QJsonValue &value : imagesJson) {
        EBImageItem::State image;
        if (!readImage(value.toObject(), &image))
            return false;
        images.append(image);
    }
    page->setImages(images);
    const QJsonValue guidesValue = object.value(QStringLiteral("guides"));
    if (!guidesValue.isUndefined()) {
        if (!guidesValue.isObject())
            return false;
        const QJsonObject guides = guidesValue.toObject();
        const QJsonValue horizontalValue = guides.value(QStringLiteral("horizontal"));
        const QJsonValue verticalValue = guides.value(QStringLiteral("vertical"));
        if (!horizontalValue.isArray() || !verticalValue.isArray())
            return false;
        const QJsonArray horizontal = horizontalValue.toArray();
        const QJsonArray vertical = verticalValue.toArray();
        if (horizontal.size() > kMaxGuidesPerAxis
            || vertical.size() > kMaxGuidesPerAxis)
            return false;
        QVector<qreal> horizontalGuides;
        QVector<qreal> verticalGuides;
        for (const QJsonValue &value : horizontal) {
            const qreal coordinate = value.toDouble(-1.0);
            if (!value.isDouble() || !qIsFinite(coordinate)
                || coordinate < 0.0 || coordinate > EBPage::Height)
                return false;
            horizontalGuides.append(coordinate);
        }
        const qreal width = EBPage::widthForSize(page->size());
        for (const QJsonValue &value : vertical) {
            const qreal coordinate = value.toDouble(-1.0);
            if (!value.isDouble() || !qIsFinite(coordinate)
                || coordinate < 0.0 || coordinate > width)
                return false;
            verticalGuides.append(coordinate);
        }
        page->setHorizontalGuides(horizontalGuides);
        page->setVerticalGuides(verticalGuides);
    }
    return true;
}

bool samePath(const QString &left, const QString &right)
{
    return QDir::cleanPath(QFileInfo(left).absoluteFilePath()).compare(
               QDir::cleanPath(QFileInfo(right).absoluteFilePath()),
               Qt::CaseInsensitive) == 0;
}

bool fileInDirectory(const QString &path, const QString &directory)
{
    const QFileInfo file(path);
    return file.isFile() && file.suffix().compare(QStringLiteral("json"),
                                                   Qt::CaseInsensitive) == 0
           && samePath(file.absolutePath(), directory);
}

bool readSummary(const QString &path, bool inTrash, EBDocumentSummary *summary)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    QJsonParseError error;
    const QJsonDocument json = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !json.isObject())
        return false;
    const QJsonObject root = json.object();
    const QJsonObject metadata = root.value(QStringLiteral("metadata")).toObject();
    const QString id = metadata.value(QStringLiteral("id")).toString();
    const QUuid uuid(id);
    const QString title = metadata.value(QStringLiteral("title")).toString();
    const QDateTime createdAt = QDateTime::fromString(
        metadata.value(QStringLiteral("createdAt")).toString(), Qt::ISODateWithMs);
    const QDateTime updatedAt = QDateTime::fromString(
        metadata.value(QStringLiteral("updatedAt")).toString(), Qt::ISODateWithMs);
    const int pageCount = metadata.value(QStringLiteral("pageCount")).toInt(-1);
    if (root.value(QStringLiteral("format")).toString()
            != QStringLiteral("EasyBoardDocument")
        || root.value(QStringLiteral("version")).toInt(-1) != 1
        || uuid.isNull() || title.isEmpty() || !createdAt.isValid()
        || !updatedAt.isValid() || pageCount < 1
        || QFileInfo(path).completeBaseName().compare(
               uuid.toString(QUuid::WithoutBraces), Qt::CaseInsensitive) != 0)
        return false;
    *summary = {uuid.toString(QUuid::WithoutBraces), title,
                QFileInfo(path).absoluteFilePath(), createdAt,
                updatedAt, pageCount, inTrash};
    return true;
}

bool moveFile(const QString &source, const QString &destination, QString *error)
{
    if (QFileInfo::exists(destination)) {
        if (error)
            *error = QStringLiteral("目标位置已存在同名文档");
        return false;
    }
    QFile file(source);
    if (!file.rename(destination)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}
}

QString EBDocumentStorage::documentsDirectory()
{
    return QDir(EBSettings::courseDataDir()).absoluteFilePath(
        QStringLiteral("documents"));
}

QString EBDocumentStorage::trashDirectory()
{
    return QDir(documentsDirectory()).absoluteFilePath(QStringLiteral("trash"));
}

QString EBDocumentStorage::documentFilePath(const EBDocument &document)
{
    return QDir(documentsDirectory()).absoluteFilePath(
        document.id() + QStringLiteral(".json"));
}

QVector<EBDocumentSummary> EBDocumentStorage::listDocuments(bool inTrash)
{
    const QString directoryPath = inTrash ? trashDirectory() : documentsDirectory();
    QDir directory(directoryPath);
    QVector<EBDocumentSummary> documents;
    for (const QFileInfo &file : directory.entryInfoList(
             {QStringLiteral("*.json")},
             QDir::Files | QDir::Readable | QDir::NoSymLinks, QDir::Name)) {
        EBDocumentSummary summary;
        if (readSummary(file.absoluteFilePath(), inTrash, &summary))
            documents.append(summary);
    }
    std::sort(documents.begin(), documents.end(),
              [](const EBDocumentSummary &left, const EBDocumentSummary &right) {
        if (left.updatedAt != right.updatedAt)
            return left.updatedAt > right.updatedAt;
        return left.id < right.id;
    });
    return documents;
}

bool EBDocumentStorage::save(const EBDocument &document, QString *savedPath,
                             QString *error)
{
    if (!QDir().mkpath(documentsDirectory())) {
        if (error)
            *error = QStringLiteral("无法创建文档目录");
        return false;
    }

    const QString path = documentFilePath(document);
    const QString recycled = QDir(trashDirectory()).absoluteFilePath(
        document.id() + QStringLiteral(".json"));
    if (!QFileInfo::exists(path) && QFileInfo::exists(recycled)) {
        if (error)
            *error = QStringLiteral("该文档位于回收站，请先恢复后再保存");
        return false;
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    if (file.write(toJson(document)) < 0
        || !file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    if (savedPath)
        *savedPath = path;
    return true;
}

bool EBDocumentStorage::load(const QString &path, EBDocument *document,
                             QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return fromJson(file.readAll(), document, error);
}

QByteArray EBDocumentStorage::toJson(const EBDocument &document)
{
    QJsonArray pages;
    for (int index = 0; index < document.pageCount(); ++index)
        pages.append(pageObject(*document.pageAt(index), index));
    const QJsonObject metadata{
        {QStringLiteral("id"), document.id()},
        {QStringLiteral("title"), document.title()},
        {QStringLiteral("createdAt"), document.createdAt().toString(Qt::ISODateWithMs)},
        {QStringLiteral("updatedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {QStringLiteral("currentPage"), document.currentPageIndex()},
        {QStringLiteral("pageCount"), document.pageCount()}
    };
    const QJsonObject root{
        {QStringLiteral("format"), QStringLiteral("EasyBoardDocument")},
        {QStringLiteral("version"), 1},
        {QStringLiteral("metadata"), metadata},
        {QStringLiteral("pages"), pages}
    };
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

bool EBDocumentStorage::fromJson(const QByteArray &data, EBDocument *document,
                                 QString *error)
{
    if (!document) {
        if (error)
            *error = QStringLiteral("文档接收对象无效");
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument json = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !json.isObject()) {
        if (error)
            *error = QStringLiteral("JSON 格式错误：%1").arg(parseError.errorString());
        return false;
    }

    const QJsonObject root = json.object();
    if (root.value(QStringLiteral("format")).toString()
            != QStringLiteral("EasyBoardDocument")
        || root.value(QStringLiteral("version")).toInt(-1) != 1) {
        if (error)
            *error = QStringLiteral("不支持的 EasyBoard 文档格式或版本");
        return false;
    }

    const QJsonObject metadata = root.value(QStringLiteral("metadata")).toObject();
    const QJsonArray pagesJson = root.value(QStringLiteral("pages")).toArray();
    const QString id = metadata.value(QStringLiteral("id")).toString();
    const QUuid uuid(id);
    const QString title = metadata.value(QStringLiteral("title")).toString();
    const QDateTime createdAt = QDateTime::fromString(
        metadata.value(QStringLiteral("createdAt")).toString(), Qt::ISODateWithMs);
    const int currentPage = metadata.value(QStringLiteral("currentPage")).toInt(-1);
    if (uuid.isNull() || title.isEmpty() || !createdAt.isValid()
        || pagesJson.isEmpty()
        || metadata.value(QStringLiteral("pageCount")).toInt(-1) != pagesJson.size()
        || currentPage < 0 || currentPage >= pagesJson.size()) {
        if (error)
            *error = QStringLiteral("文档元数据不完整");
        return false;
    }

    QVector<EBPage> pages;
    pages.reserve(pagesJson.size());
    for (int index = 0; index < pagesJson.size(); ++index) {
        EBPage page;
        if (!readPage(pagesJson.at(index).toObject(), index, &page)) {
            if (error)
                *error = QStringLiteral("第 %1 页内容无效").arg(index + 1);
            return false;
        }
        pages.append(page);
    }

    EBDocument loaded;
    loaded._id = uuid.toString(QUuid::WithoutBraces);
    loaded._title = title;
    loaded._createdAt = createdAt;
    loaded._pages = pages;
    loaded._currentPageIndex = currentPage;
    *document = loaded;
    return true;
}

bool EBDocumentStorage::renameDocument(const QString &path, const QString &title,
                                       QString *error)
{
    if (!fileInDirectory(path, documentsDirectory())) {
        if (error)
            *error = QStringLiteral("只能重命名文档列表中的文件");
        return false;
    }
    EBDocument document;
    if (!load(path, &document, error))
        return false;
    if (!samePath(path, documentFilePath(document))) {
        if (error)
            *error = QStringLiteral("文档标识与文件名不一致");
        return false;
    }
    if (!document.setTitle(title)) {
        if (error)
            *error = QStringLiteral("文档名称不能为空且不能超过 120 个字符");
        return false;
    }
    return save(document, nullptr, error);
}

bool EBDocumentStorage::duplicateDocument(const QString &path,
                                           QString *newPath, QString *error)
{
    if (!fileInDirectory(path, documentsDirectory())) {
        if (error)
            *error = QStringLiteral("只能复制文档列表中的文件");
        return false;
    }
    EBDocument document;
    if (!load(path, &document, error))
        return false;
    if (!samePath(path, documentFilePath(document))) {
        if (error)
            *error = QStringLiteral("文档标识与文件名不一致");
        return false;
    }
    document._id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    document._createdAt = QDateTime::currentDateTimeUtc();
    const QString suffix = QStringLiteral(" - 副本");
    document.setTitle(document.title().left(120 - suffix.size()) + suffix);
    return save(document, newPath, error);
}

bool EBDocumentStorage::moveToTrash(const QString &path, QString *trashPath,
                                    QString *error)
{
    if (!fileInDirectory(path, documentsDirectory())) {
        if (error)
            *error = QStringLiteral("只能回收文档列表中的文件");
        return false;
    }
    if (!QDir().mkpath(trashDirectory())) {
        if (error)
            *error = QStringLiteral("无法创建回收站目录");
        return false;
    }
    const QString destination = QDir(trashDirectory()).absoluteFilePath(
        QFileInfo(path).fileName());
    if (!moveFile(path, destination, error))
        return false;
    if (trashPath)
        *trashPath = destination;
    return true;
}

bool EBDocumentStorage::restoreFromTrash(const QString &path, QString *restoredPath,
                                         QString *error)
{
    if (!fileInDirectory(path, trashDirectory())) {
        if (error)
            *error = QStringLiteral("只能恢复回收站中的文件");
        return false;
    }
    if (!QDir().mkpath(documentsDirectory())) {
        if (error)
            *error = QStringLiteral("无法创建文档目录");
        return false;
    }
    const QString destination = QDir(documentsDirectory()).absoluteFilePath(
        QFileInfo(path).fileName());
    if (!moveFile(path, destination, error))
        return false;
    if (restoredPath)
        *restoredPath = destination;
    return true;
}

bool EBDocumentStorage::deleteFromTrash(const QString &path, QString *error)
{
    if (!fileInDirectory(path, trashDirectory())) {
        if (error)
            *error = QStringLiteral("只能永久删除回收站中的文件");
        return false;
    }
    QFile file(path);
    if (!file.remove()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}
