#ifndef EBDOCUMENTPACKAGE_H
#define EBDOCUMENTPACKAGE_H

#include <QString>

class EBDocument;

// 课程文档包压缩完整 JSON，并在导入前校验格式、大小与内容摘要。
class EBDocumentPackage
{
public:
    static QString fileDialogFilter();
    static bool exportDocument(const EBDocument &document, const QString &path,
                               QString *savedPath = nullptr,
                               QString *error = nullptr);
    static bool importDocument(const QString &path, EBDocument *document,
                               QString *error = nullptr);
};

#endif
