#include "ebdocumentstorage.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QUuid>

#include "../core/ebsettings.h"
#include "../domain/ebdocument.h"

namespace {
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
    return QJsonObject{
        {QStringLiteral("path"), pathObject(stroke.path)},
        {QStringLiteral("pen"), penObject(stroke.pen)},
        {QStringLiteral("position"), QJsonObject{
             {QStringLiteral("x"), stroke.position.x()},
             {QStringLiteral("y"), stroke.position.y()}}},
        {QStringLiteral("z"), stroke.zValue}
    };
}

QJsonObject pageObject(const EBPage &page, int index)
{
    QJsonArray strokes;
    for (const EBStrokeItem::State &stroke : page.strokes())
        strokes.append(strokeObject(stroke));
    return QJsonObject{
        {QStringLiteral("index"), index},
        {QStringLiteral("color"), colorName(page.color())},
        {QStringLiteral("pattern"), patternName(page.pattern())},
        {QStringLiteral("size"), sizeName(page.size())},
        {QStringLiteral("strokes"), strokes}
    };
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
    *stroke = {path, pen,
               QPointF(position.value(QStringLiteral("x")).toDouble(),
                       position.value(QStringLiteral("y")).toDouble()),
               object.value(QStringLiteral("z")).toDouble()};
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
    return true;
}
}

QString EBDocumentStorage::documentsDirectory()
{
    return QDir(EBSettings::courseDataDir()).absoluteFilePath(
        QStringLiteral("documents"));
}

QString EBDocumentStorage::documentFilePath(const EBDocument &document)
{
    return QDir(documentsDirectory()).absoluteFilePath(
        document.id() + QStringLiteral(".json"));
}

bool EBDocumentStorage::save(const EBDocument &document, QString *savedPath,
                             QString *error)
{
    if (!QDir().mkpath(documentsDirectory())) {
        if (error)
            *error = QStringLiteral("无法创建文档目录");
        return false;
    }

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

    const QString path = documentFilePath(document);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    if (file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) < 0
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
    if (!document) {
        if (error)
            *error = QStringLiteral("文档接收对象无效");
        return false;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument json = QJsonDocument::fromJson(file.readAll(), &parseError);
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
    const QString title = metadata.value(QStringLiteral("title")).toString();
    const QDateTime createdAt = QDateTime::fromString(
        metadata.value(QStringLiteral("createdAt")).toString(), Qt::ISODateWithMs);
    const int currentPage = metadata.value(QStringLiteral("currentPage")).toInt(-1);
    if (QUuid(id).isNull() || title.isEmpty() || !createdAt.isValid()
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
    loaded._id = id;
    loaded._title = title;
    loaded._createdAt = createdAt;
    loaded._pages = pages;
    loaded._currentPageIndex = currentPage;
    *document = loaded;
    return true;
}
