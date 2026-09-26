#include "ebdocumentpackageassets.h"

#include <QCoreApplication>

#include <QCryptographicHash>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QTemporaryDir>
#include <QtEndian>

#include "../domain/ebdocument.h"
#include "ebdocumentstorage.h"

namespace {
constexpr quint32 kMagic = 0x4542504B;
constexpr quint16 kVersion = 2;
constexpr qint64 kMaxPackageBytes = 128 * 1024 * 1024;
constexpr int kMaxDocumentBytes = 256 * 1024 * 1024;
constexpr int kMaxAssetBytes = 64 * 1024 * 1024;
constexpr int kMaxAssets = 1000;

bool validName(const QString &name)
{
    static const QRegularExpression pattern(QStringLiteral("^[0-9a-f]{64}\\.png$"));
    return pattern.match(name).hasMatch();
}

QSet<QString> assetNames(const QByteArray &json, bool *valid)
{
    const QJsonDocument document = QJsonDocument::fromJson(json);
    *valid = document.isObject();
    QSet<QString> names;
    if (!*valid)
        return names;
    const QJsonArray pages = document.object().value(QStringLiteral("pages")).toArray();
    for (const QJsonValue &value : pages) {
        const QJsonObject background = value.toObject()
            .value(QStringLiteral("backgroundImage")).toObject();
        if (!background.contains(QStringLiteral("file")))
            continue;
        const QString name = background.value(QStringLiteral("file")).toString();
        if (!validName(name)) {
            *valid = false;
            return {};
        }
        names.insert(name);
    }
    *valid = names.size() <= kMaxAssets;
    return names;
}

bool copyAsset(QIODevice *source, QIODevice *destination, qint64 size,
               QCryptographicHash *digest)
{
    QByteArray buffer(65536, '\0');
    while (size > 0) {
        const qint64 length = qMin(size, qint64(buffer.size()));
        if (source->read(buffer.data(), length) != length
            || destination->write(buffer.constData(), length) != length)
            return false;
        if (digest)
            digest->addData(buffer.constData(), length);
        size -= length;
    }
    return true;
}
}

bool EBDocumentPackageAssets::exportDocument(const EBDocument &document,
                                              const QString &path,
                                              QString *savedPath, QString *error)
{
    QTemporaryDir assets;
    if (!assets.isValid()) {
        if (error)
            *error = QCoreApplication::translate("EBDocumentPackageAssets", "无法创建文档包临时目录");
        return false;
    }
    const QByteArray json = EBDocumentStorage::toJson(document, assets.path(), error);
    if (json.isEmpty() || json.size() > kMaxDocumentBytes) {
        if (error && error->isEmpty())
            *error = QCoreApplication::translate("EBDocumentPackageAssets", "文档内容为空或超过 256 MB");
        return false;
    }
    bool valid = false;
    const QSet<QString> names = assetNames(json, &valid);
    if (!valid) {
        if (error)
            *error = QCoreApplication::translate("EBDocumentPackageAssets", "文档背景图像索引无效");
        return false;
    }
    const QByteArray compressed = qCompress(json, 6);
    if (compressed.isEmpty())
        return false;
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    QDataStream output(&file);
    output.setByteOrder(QDataStream::BigEndian);
    const QByteArray digest = QCryptographicHash::hash(json, QCryptographicHash::Sha256);
    output << kMagic << kVersion;
    output.writeRawData(digest.constData(), digest.size());
    output << quint32(compressed.size());
    output.writeRawData(compressed.constData(), compressed.size());
    output << quint32(names.size());
    QStringList sorted = names.values();
    sorted.sort();
    for (const QString &name : sorted) {
        QFile asset(QDir(assets.path()).filePath(name));
        if (!asset.open(QIODevice::ReadOnly) || asset.size() <= 0
            || asset.size() > kMaxAssetBytes
            || file.pos() + 2 + name.size() + 4 + asset.size() > kMaxPackageBytes) {
            if (error)
                *error = QCoreApplication::translate("EBDocumentPackageAssets", "文档包图像缺失或超过大小限制");
            return false;
        }
        const QByteArray encoded = name.toLatin1();
        output << quint16(encoded.size());
        output.writeRawData(encoded.constData(), encoded.size());
        output << quint32(asset.size());
        QCryptographicHash hash(QCryptographicHash::Sha256);
        if (!copyAsset(&asset, &file, asset.size(), &hash)
            || QString::fromLatin1(hash.result().toHex())
                != name.left(64)) {
            if (error)
                *error = QCoreApplication::translate("EBDocumentPackageAssets", "文档包图像写入失败或内容损坏");
            return false;
        }
    }
    if (output.status() != QDataStream::Ok || file.size() > kMaxPackageBytes
        || !file.commit()) {
        if (error)
            *error = QCoreApplication::translate("EBDocumentPackageAssets", "课程文档包写入失败");
        return false;
    }
    if (savedPath)
        *savedPath = path;
    return true;
}

bool EBDocumentPackageAssets::importDocument(const QString &path,
                                              EBDocument *document,
                                              QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > kMaxPackageBytes)
        return false;
    QDataStream input(&file);
    input.setByteOrder(QDataStream::BigEndian);
    quint32 magic = 0;
    quint16 version = 0;
    input >> magic >> version;
    QByteArray digest(32, '\0');
    quint32 compressedSize = 0;
    if (magic != kMagic || version != kVersion
        || input.readRawData(digest.data(), digest.size()) != digest.size())
        return false;
    input >> compressedSize;
    if (compressedSize < 4 || compressedSize > kMaxPackageBytes - file.pos())
        return false;
    QByteArray compressed(int(compressedSize), '\0');
    if (input.readRawData(compressed.data(), compressed.size())
        != compressed.size())
        return false;
    const quint32 declaredSize = qFromBigEndian<quint32>(
        reinterpret_cast<const uchar *>(compressed.constData()));
    if (declaredSize == 0 || declaredSize > kMaxDocumentBytes)
        return false;
    const QByteArray json = qUncompress(compressed);
    if (json.size() != int(declaredSize)
        || QCryptographicHash::hash(json, QCryptographicHash::Sha256) != digest) {
        if (error)
            *error = QCoreApplication::translate("EBDocumentPackageAssets", "文档包内容不完整或已损坏");
        return false;
    }
    bool valid = false;
    QSet<QString> required = assetNames(json, &valid);
    quint32 count = 0;
    input >> count;
    if (!valid || count != quint32(required.size()))
        return false;
    QTemporaryDir assets;
    if (!assets.isValid())
        return false;
    for (quint32 index = 0; index < count; ++index) {
        quint16 nameSize = 0;
        quint32 bytes = 0;
        input >> nameSize;
        if (nameSize != 68)
            return false;
        QByteArray encoded(nameSize, '\0');
        if (input.readRawData(encoded.data(), encoded.size()) != encoded.size())
            return false;
        const QString name = QString::fromLatin1(encoded);
        input >> bytes;
        if (!required.remove(name) || bytes == 0 || bytes > kMaxAssetBytes
            || bytes > file.size() - file.pos())
            return false;
        QSaveFile asset(QDir(assets.path()).filePath(name));
        QCryptographicHash hash(QCryptographicHash::Sha256);
        if (!asset.open(QIODevice::WriteOnly)
            || !copyAsset(&file, &asset, bytes, &hash)
            || QString::fromLatin1(hash.result().toHex()) != name.left(64)
            || !asset.commit()) {
            if (error)
                *error = QCoreApplication::translate("EBDocumentPackageAssets", "文档包图像不完整或已损坏");
            return false;
        }
    }
    if (input.status() != QDataStream::Ok || !required.isEmpty() || !file.atEnd())
        return false;
    return EBDocumentStorage::fromJson(json, document, error, assets.path());
}
