#ifndef EBDOCUMENTSTORAGE_H
#define EBDOCUMENTSTORAGE_H

#include <QString>

class EBDocument;

// 教程文档使用独立 JSON 文件保存，不读写原 WBoard 的文档目录。
class EBDocumentStorage
{
public:
    static QString documentsDirectory();
    static QString documentFilePath(const EBDocument &document);
    static bool save(const EBDocument &document, QString *savedPath = nullptr,
                     QString *error = nullptr);
    static bool load(const QString &path, EBDocument *document,
                     QString *error = nullptr);
};

#endif
