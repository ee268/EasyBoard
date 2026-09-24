#ifndef EBDOCUMENTPACKAGE_H
#define EBDOCUMENTPACKAGE_H

#include <QString>

class EBDocument;

// 课程文档包支持旧版完整 JSON 和新版独立背景图像资源。
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
