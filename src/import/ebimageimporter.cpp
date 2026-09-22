#include "ebimageimporter.h"

#include <QCoreApplication>
#include <QBuffer>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QSet>

#include <algorithm>
#include <cmath>

#include "../domain/ebdocument.h"

namespace {
constexpr qint64 kMaxImageFileBytes = 64 * 1024 * 1024;
constexpr qint64 kMaxImagePixels = 40000000;
}

QString EBImageImporter::fileDialogFilter()
{
    QSet<QString> uniqueFormats;
    for (const QByteArray &format : QImageReader::supportedImageFormats())
        uniqueFormats.insert(QString::fromLatin1(format).toLower());
    uniqueFormats.insert(QStringLiteral("svg"));
    QStringList formats = uniqueFormats.values();
    std::sort(formats.begin(), formats.end());
    QStringList patterns;
    for (const QString &format : formats)
        patterns.append(QStringLiteral("*.%1").arg(format));
    return QCoreApplication::translate(
        "EBImageImporter", "图片文件 (%1)").arg(patterns.join(QLatin1Char(' ')));
}

bool EBImageImporter::importFile(const QString &path, EBDocument *document,
                                 QString *error)
{
    if (!document) {
        if (error)
            *error = QCoreApplication::translate(
                "EBImageImporter", "文档接收对象无效");
        return false;
    }

    const QFileInfo file(path);
    if (!file.isFile()) {
        if (error)
            *error = QCoreApplication::translate(
                "EBImageImporter", "图片文件不存在");
        return false;
    }
    if (file.size() <= 0 || file.size() > kMaxImageFileBytes) {
        if (error)
            *error = QCoreApplication::translate(
                "EBImageImporter", "图片文件为空或超过 64 MB");
        return false;
    }

    QImageReader reader(file.absoluteFilePath());
    reader.setAutoTransform(true);
    const QSize size = reader.size();
    if (!reader.canRead() || !size.isValid()) {
        if (error)
            *error = QCoreApplication::translate(
                "EBImageImporter", "不支持该图片格式：%1").arg(reader.errorString());
        return false;
    }
    if (qint64(size.width()) * qint64(size.height()) > kMaxImagePixels) {
        if (error)
            *error = QCoreApplication::translate(
                "EBImageImporter", "图片像素数量超过 4000 万");
        return false;
    }

    QImage image = reader.read();
    if (image.isNull()) {
        if (error)
            *error = QCoreApplication::translate(
                "EBImageImporter", "图片解码失败：%1").arg(reader.errorString());
        return false;
    }
    image = image.convertToFormat(QImage::Format_ARGB32);

    EBDocument imported;
    if (!imported.setTitle(file.completeBaseName()))
        imported.setTitle(QCoreApplication::translate(
            "EBImageImporter", "导入图片"));
    EBPage *page = imported.currentPage();
    const qreal imageRatio = qreal(image.width()) / qreal(image.height());
    const qreal standardDistance = std::abs(imageRatio - 4.0 / 3.0);
    const qreal widescreenDistance = std::abs(imageRatio - 16.0 / 9.0);
    page->setSize(widescreenDistance < standardDistance
                      ? EBPage::Size::Widescreen
                      : EBPage::Size::Standard);
    page->setBackgroundImage(image);
    *document = imported;
    return true;
}

bool EBImageImporter::loadObject(const QString &path,
                                 EBImageItem::State *state, QString *error)
{
    if (!state) {
        if (error)
            *error = QCoreApplication::translate(
                "EBImageImporter", "图片对象接收参数无效");
        return false;
    }
    const QFileInfo file(path);
    if (!file.isFile() || file.size() <= 0
        || file.size() > kMaxImageFileBytes) {
        if (error)
            *error = QCoreApplication::translate(
                "EBImageImporter", "图片文件不存在、为空或超过 64 MB");
        return false;
    }

    EBImageItem::State result;
    QSizeF naturalSize;
    if (file.suffix().compare(QStringLiteral("svg"),
                              Qt::CaseInsensitive) == 0) {
        QFile input(file.absoluteFilePath());
        if (!input.open(QIODevice::ReadOnly)) {
            if (error)
                *error = input.errorString();
            return false;
        }
        result.format = EBImageItem::Format::Svg;
        result.data = input.readAll();
        if (!EBImageItem::naturalSize(result.format, result.data, &naturalSize)) {
            if (error)
                *error = QCoreApplication::translate(
                    "EBImageImporter", "SVG 文件无效或尺寸超过限制");
            return false;
        }
    } else {
        QImageReader reader(file.absoluteFilePath());
        reader.setAutoTransform(true);
        const QSize size = reader.size();
        if (!reader.canRead() || !size.isValid()
            || qint64(size.width()) * qint64(size.height()) > kMaxImagePixels) {
            if (error)
                *error = QCoreApplication::translate(
                    "EBImageImporter", "图片格式无效或像素数量超过限制");
            return false;
        }
        const QImage image = reader.read().convertToFormat(QImage::Format_ARGB32);
        if (image.isNull()) {
            if (error)
                *error = QCoreApplication::translate(
                    "EBImageImporter", "图片解码失败：%1").arg(reader.errorString());
            return false;
        }
        QBuffer buffer(&result.data);
        buffer.open(QIODevice::WriteOnly);
        if (!image.save(&buffer, "PNG")) {
            if (error)
                *error = QCoreApplication::translate(
                    "EBImageImporter", "图片转换为 PNG 失败");
            return false;
        }
        result.format = EBImageItem::Format::Png;
        naturalSize = image.size();
    }
    result.size = naturalSize;
    result.transformOrigin = QPointF(naturalSize.width() / 2.0,
                                     naturalSize.height() / 2.0);
    *state = result;
    return true;
}
