#ifndef EBDOCUMENTBACKUP_H
#define EBDOCUMENTBACKUP_H

#include <QByteArray>
#include <QSet>
#include <QString>

class EBDocumentBackup
{
public:
    static QString pathFor(const QString &documentPath);
    static bool snapshot(const QString &documentPath,
                         const QString &assetDirectory, QString *error);
    static QByteArray read(const QString &documentPath);
    static QSet<QString> referencedAssets(const QString &documentPath);
    static bool move(const QString &source, const QString &destination,
                     QString *error);
    static void remove(const QString &documentPath);
};

#endif
