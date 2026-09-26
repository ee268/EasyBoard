#include "ebdocumentexporter.h"

#include <QCoreApplication>

#include <QFileInfo>
#include <QImageWriter>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QSaveFile>
#include <QtMath>

#include "../board/ebboardscene.h"
#include "../domain/ebdocument.h"

namespace {
constexpr int kPdfResolution = 144;

QSize pagePixelSize(const EBPage &page)
{
    return QSize(qRound(page.pageWidth()), qRound(page.pageHeight()));
}

QPageSize pdfPageSize(const EBPage &page)
{
    const QSize pixels = pagePixelSize(page);
    return QPageSize(pixels / 2, QStringLiteral("EasyBoard"),
                     QPageSize::ExactMatch);
}

QString imageOutputPath(const QString &path, QByteArray *format, QString *error)
{
    if (path.trimmed().isEmpty()) {
        if (error)
            *error = QCoreApplication::translate("EBDocumentExporter", "图片导出路径不能为空");
        return QString();
    }
    QString result = QFileInfo(path).absoluteFilePath();
    QString suffix = QFileInfo(result).suffix().toLower();
    if (suffix.isEmpty()) {
        result += QStringLiteral(".png");
        suffix = QStringLiteral("png");
    }
    if (suffix != QStringLiteral("png") && suffix != QStringLiteral("jpg")
        && suffix != QStringLiteral("jpeg")) {
        if (error)
            *error = QCoreApplication::translate("EBDocumentExporter", "当前页只能导出为 PNG 或 JPEG 图片");
        return QString();
    }
    *format = suffix.toLatin1();
    return result;
}

QString pdfOutputPath(const QString &path, QString *error)
{
    if (path.trimmed().isEmpty()) {
        if (error)
            *error = QCoreApplication::translate("EBDocumentExporter", "PDF 导出路径不能为空");
        return QString();
    }
    QString result = QFileInfo(path).absoluteFilePath();
    const QString suffix = QFileInfo(result).suffix();
    if (suffix.isEmpty())
        return result + QStringLiteral(".pdf");
    if (suffix.compare(QStringLiteral("pdf"), Qt::CaseInsensitive) != 0) {
        if (error)
            *error = QCoreApplication::translate("EBDocumentExporter", "整份文档只能导出为 PDF 文件");
        return QString();
    }
    return result;
}

void renderPageToPainter(const EBPage &page, QPainter *painter,
                         const QRectF &target)
{
    EBBoardScene scene;
    scene.showPage(page);
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setRenderHint(QPainter::SmoothPixmapTransform);
    scene.render(painter, target, scene.pageRect(), Qt::IgnoreAspectRatio);
}
}

QImage EBDocumentExporter::renderPage(const EBPage &page)
{
    const QSize size = pagePixelSize(page);
    QImage image(size, QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    renderPageToPainter(page, &painter, QRectF(QPointF(), size));
    painter.end();
    return image;
}

bool EBDocumentExporter::exportPageImage(const EBPage &page, const QString &path,
                                         QString *savedPath, QString *error)
{
    QByteArray format;
    const QString output = imageOutputPath(path, &format, error);
    if (output.isEmpty())
        return false;

    QSaveFile file(output);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    QImageWriter writer(&file, format);
    if (format == QByteArrayLiteral("jpg")
        || format == QByteArrayLiteral("jpeg"))
        writer.setQuality(92);
    if (!writer.write(renderPage(page))) {
        if (error)
            *error = writer.errorString();
        return false;
    }
    if (!file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    if (savedPath)
        *savedPath = output;
    return true;
}

bool EBDocumentExporter::exportDocumentPdf(const EBDocument &document,
                                           const QString &path,
                                           QString *savedPath, QString *error)
{
    const QString output = pdfOutputPath(path, error);
    if (output.isEmpty())
        return false;
    QSaveFile file(output);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }

    bool success = true;
    {
        QPdfWriter writer(&file);
        writer.setResolution(kPdfResolution);
        writer.setTitle(document.title());
        writer.setCreator(QStringLiteral("EasyBoard"));
        writer.setPageSize(pdfPageSize(*document.pageAt(0)));
        writer.setPageMargins(QMarginsF(0, 0, 0, 0));
        QPainter painter(&writer);
        if (!painter.isActive()) {
            success = false;
        } else {
            for (int index = 0; index < document.pageCount(); ++index) {
                const EBPage *page = document.pageAt(index);
                if (index > 0) {
                    writer.setPageSize(pdfPageSize(*page));
                    writer.setPageMargins(QMarginsF(0, 0, 0, 0));
                    if (!writer.newPage()) {
                        success = false;
                        break;
                    }
                }
                renderPageToPainter(*page, &painter,
                                    QRectF(0, 0, writer.width(), writer.height()));
            }
            painter.end();
        }
    }
    if (!success) {
        if (error)
            *error = QCoreApplication::translate("EBDocumentExporter", "PDF 页面创建失败");
        return false;
    }
    if (!file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    if (savedPath)
        *savedPath = output;
    return true;
}
