#ifndef EBDOCUMENTEXPORTER_H
#define EBDOCUMENTEXPORTER_H

#include <QImage>
#include <QString>

class EBDocument;
class EBPage;

// 导出器只读取文档模型，通过画板场景生成图片或 PDF，不修改当前文档。
class EBDocumentExporter
{
public:
    static QImage renderPage(const EBPage &page);
    static bool exportPageImage(const EBPage &page, const QString &path,
                                QString *savedPath = nullptr,
                                QString *error = nullptr);
    static bool exportDocumentPdf(const EBDocument &document, const QString &path,
                                  QString *savedPath = nullptr,
                                  QString *error = nullptr);
};

#endif
