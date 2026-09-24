#include "ebdesktopinkstore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QScreen>
#include <QtMath>

#include "../core/ebsettings.h"

namespace {
constexpr int kMaxScreens = 16;
constexpr int kMaxStrokes = 10000;
constexpr int kMaxElements = 50000;
constexpr int kMaxBytes = 32 * 1024 * 1024;

QString inkPath()
{
    return QDir(EBSettings::userDataDir()).filePath(
        QStringLiteral("desktop/ink.json"));
}

QJsonArray pathData(const QPainterPath &path)
{
    QJsonArray elements;
    for (int index = 0; index < path.elementCount(); ++index) {
        const QPainterPath::Element element = path.elementAt(index);
        elements.append(QJsonArray{int(element.type), element.x, element.y});
    }
    return elements;
}

bool readPath(const QJsonArray &elements, QPainterPath *path)
{
    if (elements.isEmpty() || elements.size() > kMaxElements)
        return false;
    QPainterPath result;
    for (int index = 0; index < elements.size(); ++index) {
        const QJsonArray point = elements.at(index).toArray();
        if (point.size() != 3 || !point.at(0).isDouble()
            || !point.at(1).isDouble() || !point.at(2).isDouble())
            return false;
        const int type = point.at(0).toInt(-1);
        const qreal x = point.at(1).toDouble();
        const qreal y = point.at(2).toDouble();
        if (!qIsFinite(x) || !qIsFinite(y)
            || qAbs(x) > 100000.0 || qAbs(y) > 100000.0)
            return false;
        if (type == int(QPainterPath::MoveToElement)) {
            result.moveTo(x, y);
        } else if (type == int(QPainterPath::LineToElement) && index > 0) {
            result.lineTo(x, y);
        } else if (type == int(QPainterPath::CurveToElement)
                   && index > 0 && index + 2 < elements.size()) {
            const QJsonArray second = elements.at(index + 1).toArray();
            const QJsonArray third = elements.at(index + 2).toArray();
            if (second.size() != 3 || third.size() != 3
                || second.at(0).toInt(-1)
                    != int(QPainterPath::CurveToDataElement)
                || third.at(0).toInt(-1)
                    != int(QPainterPath::CurveToDataElement))
                return false;
            const qreal sx = second.at(1).toDouble();
            const qreal sy = second.at(2).toDouble();
            const qreal tx = third.at(1).toDouble();
            const qreal ty = third.at(2).toDouble();
            if (!qIsFinite(sx) || !qIsFinite(sy) || !qIsFinite(tx)
                || !qIsFinite(ty))
                return false;
            result.cubicTo(x, y, sx, sy, tx, ty);
            index += 2;
        } else {
            return false;
        }
    }
    *path = result;
    return true;
}
}

QString EBDesktopInkStore::screenId(const QScreen *screen)
{
    if (!screen)
        return {};
    if (!screen->name().isEmpty())
        return screen->name();
    const QString hardware = screen->manufacturer() + QLatin1Char('/')
        + screen->model() + QLatin1Char('/') + screen->serialNumber();
    return hardware != QStringLiteral("//") ? hardware
        : QStringLiteral("%1,%2").arg(screen->geometry().x())
              .arg(screen->geometry().y());
}

QHash<QString, EBDesktopInkStore::ScreenInk> EBDesktopInkStore::load()
{
    QFile file(inkPath());
    if (!file.open(QIODevice::ReadOnly) || file.size() > kMaxBytes)
        return {};
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    if (root.value(QStringLiteral("version")).toInt() != 1)
        return {};
    const QJsonArray screens = root.value(QStringLiteral("screens")).toArray();
    if (screens.size() > kMaxScreens)
        return {};
    QHash<QString, ScreenInk> result;
    for (const QJsonValue &screenValue : screens) {
        const QJsonObject object = screenValue.toObject();
        const QString id = object.value(QStringLiteral("id")).toString();
        const int width = object.value(QStringLiteral("width")).toInt();
        const int height = object.value(QStringLiteral("height")).toInt();
        const QJsonArray strokes = object.value(QStringLiteral("strokes")).toArray();
        const int applied = object.value(QStringLiteral("applied")).toInt(-1);
        if (id.isEmpty() || id.size() > 256 || width < 1 || height < 1
            || width > 20000 || height > 20000 || strokes.size() > kMaxStrokes
            || applied < 0 || applied > strokes.size())
            continue;
        ScreenInk ink;
        ink.size = QSize(width, height);
        ink.applied = applied;
        bool valid = true;
        for (const QJsonValue &strokeValue : strokes) {
            const QJsonObject stored = strokeValue.toObject();
            const int tool = stored.value(QStringLiteral("tool")).toInt(-1);
            const QColor color(stored.value(QStringLiteral("color")).toString());
            const qreal lineWidth = stored.value(QStringLiteral("width")).toDouble();
            QPainterPath path;
            if (tool < int(Tool::Pen) || tool > int(Tool::Ellipse)
                || !color.isValid() || !qIsFinite(lineWidth)
                || lineWidth < 1.0 || lineWidth > 48.0
                || !readPath(stored.value(QStringLiteral("path")).toArray(),
                             &path)) {
                valid = false;
                break;
            }
            ink.strokes.append({static_cast<Tool>(tool), path, color, lineWidth});
        }
        if (valid)
            result.insert(id, ink);
    }
    return result;
}

bool EBDesktopInkStore::save(const QHash<QString, ScreenInk> &screens)
{
    QJsonArray storedScreens;
    for (auto iterator = screens.cbegin(); iterator != screens.cend(); ++iterator) {
        const ScreenInk &ink = iterator.value();
        if (!ink.size.isValid() || ink.strokes.isEmpty())
            continue;
        if (storedScreens.size() >= kMaxScreens
            || ink.strokes.size() > kMaxStrokes)
            return false;
        QJsonArray strokes;
        for (const Stroke &stroke : ink.strokes) {
            if (stroke.path.elementCount() > kMaxElements)
                return false;
            strokes.append(QJsonObject{
                {QStringLiteral("tool"), int(stroke.tool)},
                {QStringLiteral("color"), stroke.color.name(QColor::HexArgb)},
                {QStringLiteral("width"), stroke.width},
                {QStringLiteral("path"), pathData(stroke.path)}
            });
        }
        storedScreens.append(QJsonObject{
            {QStringLiteral("id"), iterator.key()},
            {QStringLiteral("width"), ink.size.width()},
            {QStringLiteral("height"), ink.size.height()},
            {QStringLiteral("applied"), ink.applied},
            {QStringLiteral("strokes"), strokes}
        });
    }
    const QByteArray data = QJsonDocument(QJsonObject{
        {QStringLiteral("version"), 1},
        {QStringLiteral("screens"), storedScreens}
    }).toJson(QJsonDocument::Compact);
    if (data.size() > kMaxBytes)
        return false;
    const QString path = inkPath();
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return false;
    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly)
        && file.write(data) == data.size() && file.commit();
}
