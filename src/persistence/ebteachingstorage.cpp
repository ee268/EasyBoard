#include "ebteachingstorage.h"

#include <QJsonObject>
#include <QtMath>

namespace {
QString nameForKind(EBTeachingState::Kind kind)
{
    switch (kind) {
    case EBTeachingState::Kind::Ruler: return QStringLiteral("ruler");
    case EBTeachingState::Kind::Triangle45: return QStringLiteral("triangle45");
    case EBTeachingState::Kind::Triangle30: return QStringLiteral("triangle30");
    case EBTeachingState::Kind::Protractor: return QStringLiteral("protractor");
    case EBTeachingState::Kind::Compass: return QStringLiteral("compass");
    case EBTeachingState::Kind::Curtain: return QStringLiteral("curtain");
    case EBTeachingState::Kind::Spotlight: return QStringLiteral("spotlight");
    case EBTeachingState::Kind::Magnifier: return QStringLiteral("magnifier");
    default: return QString();
    }
}

EBTeachingState::Kind kindForName(const QString &name)
{
    for (int index = int(EBTeachingState::Kind::Ruler);
         index <= int(EBTeachingState::Kind::Magnifier); ++index) {
        const auto kind = static_cast<EBTeachingState::Kind>(index);
        if (nameForKind(kind) == name)
            return kind;
    }
    return EBTeachingState::Kind::None;
}

bool readNumber(const QJsonObject &object, const char *key,
                qreal minimum, qreal maximum, qreal *result)
{
    const QJsonValue value = object.value(QLatin1String(key));
    if (!value.isDouble())
        return false;
    const qreal number = value.toDouble();
    if (!qIsFinite(number) || number < minimum || number > maximum)
        return false;
    *result = number;
    return true;
}
}

QJsonArray EBTeachingStorage::toJson(const EBPage::TeachingTools &tools)
{
    QJsonArray result;
    for (const EBTeachingState &tool : tools) {
        result.append(QJsonObject{
            {QStringLiteral("kind"), nameForKind(tool.kind)},
            {QStringLiteral("centerX"), tool.center.x()},
            {QStringLiteral("centerY"), tool.center.y()},
            {QStringLiteral("frameX"), tool.frame.x()},
            {QStringLiteral("frameY"), tool.frame.y()},
            {QStringLiteral("frameWidth"), tool.frame.width()},
            {QStringLiteral("frameHeight"), tool.frame.height()},
            {QStringLiteral("rotation"), tool.rotation},
            {QStringLiteral("scale"), tool.scale},
            {QStringLiteral("radius"), tool.radius},
            {QStringLiteral("angle"), tool.measuredAngle},
            {QStringLiteral("magnification"), tool.magnification},
            {QStringLiteral("flipHorizontal"), tool.flipHorizontal},
            {QStringLiteral("flipVertical"), tool.flipVertical},
            {QStringLiteral("rectangularLens"), tool.rectangularLens}
        });
    }
    return result;
}

bool EBTeachingStorage::fromJson(const QJsonValue &value,
                                 EBPage::TeachingTools *tools,
                                 const QSizeF &pageSize)
{
    tools->clear();
    if (value.isUndefined())
        return true;
    if (!value.isArray() || value.toArray().size() > 32)
        return false;
    for (const QJsonValue &entry : value.toArray()) {
        if (!entry.isObject())
            return false;
        const QJsonObject object = entry.toObject();
        EBTeachingState state;
        state.kind = kindForName(object.value(QStringLiteral("kind")).toString());
        qreal centerX, centerY, frameX, frameY, width, height;
        if (state.kind == EBTeachingState::Kind::None
            || !readNumber(object, "centerX", 0.0, pageSize.width(), &centerX)
            || !readNumber(object, "centerY", 0.0, pageSize.height(), &centerY)
            || !readNumber(object, "frameX", -pageSize.width(),
                           pageSize.width(), &frameX)
            || !readNumber(object, "frameY", -pageSize.height(),
                           pageSize.height(), &frameY)
            || !readNumber(object, "frameWidth", 0.0,
                           pageSize.width() * 2.0, &width)
            || !readNumber(object, "frameHeight", 0.0,
                           pageSize.height() * 2.0, &height)
            || !readNumber(object, "rotation", -36000.0, 36000.0,
                           &state.rotation)
            || !readNumber(object, "scale", 0.5, 3.0, &state.scale)
            || !readNumber(object, "radius", 1.0,
                           pageSize.width(), &state.radius)
            || !readNumber(object, "angle", 0.0, 180.0,
                           &state.measuredAngle)
            || !readNumber(object, "magnification", 1.0, 8.0,
                           &state.magnification))
            return false;
        state.center = QPointF(centerX, centerY);
        state.frame = QRectF(frameX, frameY, width, height);
        state.flipHorizontal = object.value(
            QStringLiteral("flipHorizontal")).toBool(false);
        state.flipVertical = object.value(
            QStringLiteral("flipVertical")).toBool(false);
        state.rectangularLens = object.value(
            QStringLiteral("rectangularLens")).toBool(false);
        tools->append(state);
    }
    return true;
}
