#ifndef EBDOCUMENTPACKAGEASSETS_H
#define EBDOCUMENTPACKAGEASSETS_H

#include <QString>

class EBDocument;

// 文档包第二版将页面背景图像独立写入，按文件流读取并逐一校验摘要。
class EBDocumentPackageAssets
{
public:
    static bool exportDocument(const EBDocument &document, const QString &path,
                               QString *savedPath, QString *error);
    static bool importDocument(const QString &path, EBDocument *document,
                               QString *error);
};

#endif
