#ifndef EBDOCUMENTSTORAGE_H
#define EBDOCUMENTSTORAGE_H

#include <QDateTime>
#include <QByteArray>
#include <QString>
#include <QVector>

class EBDocument;

struct EBDocumentSummary
{
    QString id;
    QString title;
    QString path;
    QDateTime createdAt;
    QDateTime updatedAt;
    int pageCount;
    bool inTrash;
};

// 教程文档使用独立 JSON 文件保存，不读写原 WBoard 的文档目录。
class EBDocumentStorage
{
public:
    static QString documentsDirectory();
    static QString trashDirectory();
    static QString documentFilePath(const EBDocument &document);
    static QVector<EBDocumentSummary> listDocuments(bool inTrash = false);
    static QByteArray toJson(const EBDocument &document);
    static bool fromJson(const QByteArray &data, EBDocument *document,
                         QString *error = nullptr);
    static bool save(const EBDocument &document, QString *savedPath = nullptr,
                     QString *error = nullptr);
    static bool load(const QString &path, EBDocument *document,
                     QString *error = nullptr);
    static bool renameDocument(const QString &path, const QString &title,
                               QString *error = nullptr);
    static bool moveToTrash(const QString &path, QString *trashPath = nullptr,
                            QString *error = nullptr);
    static bool restoreFromTrash(const QString &path, QString *restoredPath = nullptr,
                                 QString *error = nullptr);
    static bool deleteFromTrash(const QString &path, QString *error = nullptr);
};

#endif
