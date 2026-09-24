#include "ebdocumentbackup.h"
#include "ebdocumentstorage.h"

#include "../domain/ebdocument.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>

namespace {
constexpr qint64 kMaxDocumentBytes = 256 * 1024 * 1024;

bool validAssetName(const QString &name)
{
    static const QRegularExpression pattern(QStringLiteral("^[0-9a-f]{64}\\.png$"));
    return pattern.match(name).hasMatch();
}

QSet<QString> namesIn(const QByteArray &data)
{
    QSet<QString> names;
    const QJsonArray pages = QJsonDocument::fromJson(data).object()
        .value(QStringLiteral("pages")).toArray();
    for (const QJsonValue &value : pages) {
        const QString name = value.toObject()
            .value(QStringLiteral("backgroundImage")).toObject()
            .value(QStringLiteral("file")).toString();
        if (validAssetName(name))
            names.insert(name);
    }
    return names;
}
}

QString EBDocumentBackup::pathFor(const QString &documentPath)
{
    return documentPath + QStringLiteral(".bak");
}

bool EBDocumentBackup::snapshot(const QString &documentPath,
                                const QString &assetDirectory, QString *error)
{
    if (!QFileInfo::exists(documentPath))
        return true;
    QFile source(documentPath);
    if (!source.open(QIODevice::ReadOnly)
        || source.size() <= 0 || source.size() > kMaxDocumentBytes) {
        if (error)
            *error = QStringLiteral("无法读取当前文档以创建备份");
        return false;
    }
    const QByteArray data = source.readAll();
    EBDocument existing;
    if (!EBDocumentStorage::fromJson(data, &existing, nullptr, assetDirectory))
        return true;
    QSaveFile backup(pathFor(documentPath));
    if (!backup.open(QIODevice::WriteOnly)
        || backup.write(data) != data.size() || !backup.commit()) {
        if (error)
            *error = QStringLiteral("无法创建文档备份");
        return false;
    }
    return true;
}

QByteArray EBDocumentBackup::read(const QString &documentPath)
{
    QFile file(pathFor(documentPath));
    if (!file.open(QIODevice::ReadOnly)
        || file.size() <= 0 || file.size() > kMaxDocumentBytes)
        return {};
    return file.readAll();
}

QSet<QString> EBDocumentBackup::referencedAssets(const QString &documentPath)
{
    return namesIn(read(documentPath));
}

bool EBDocumentBackup::move(const QString &source,
                            const QString &destination, QString *error)
{
    const QString oldPath = pathFor(source);
    if (!QFileInfo::exists(oldPath))
        return true;
    const QString newPath = pathFor(destination);
    QFile backup(oldPath);
    if (QFileInfo::exists(newPath) || !backup.rename(newPath)) {
        if (error)
            *error = QStringLiteral("无法移动文档备份");
        return false;
    }
    return true;
}

void EBDocumentBackup::remove(const QString &documentPath)
{
    QFile::remove(pathFor(documentPath));
}
