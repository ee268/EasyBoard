#include "ebdocumentpackage.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QUuid>
#include <QtEndian>

#include "../domain/ebdocument.h"
#include "ebdocumentstorage.h"
#include "ebdocumentpackageassets.h"

namespace {
constexpr quint32 kPackageMagic = 0x4542504B; // EBPK
constexpr quint16 kPackageVersion = 1;
constexpr int kMaxPackageBytes = 128 * 1024 * 1024;
constexpr quint32 kMaxDocumentBytes = 256 * 1024 * 1024;
constexpr int kDigestBytes = 32;
constexpr int kHeaderBytes = 4 + int(sizeof(quint16)) + kDigestBytes;

QString outputPath(const QString &path, QString *error)
{
    if (path.trimmed().isEmpty()) {
        if (error)
            *error = QStringLiteral("文档包路径不能为空");
        return QString();
    }
    QString result = QFileInfo(path).absoluteFilePath();
    const QString suffix = QFileInfo(result).suffix();
    if (suffix.isEmpty())
        return result + QStringLiteral(".ebz");
    if (suffix.compare(QStringLiteral("ebz"), Qt::CaseInsensitive) != 0) {
        if (error)
            *error = QStringLiteral("课程文档包必须使用 .ebz 扩展名");
        return QString();
    }
    return result;
}
}

QString EBDocumentPackage::fileDialogFilter()
{
    return QCoreApplication::translate(
        "EBDocumentPackage", "EasyBoard 课程文档包 (*.ebz)");
}

bool EBDocumentPackage::exportDocument(const EBDocument &document,
                                       const QString &path,
                                       QString *savedPath, QString *error)
{
    const QString output = outputPath(path, error);
    if (output.isEmpty())
        return false;
    return EBDocumentPackageAssets::exportDocument(document, output,
                                                   savedPath, error);
}

bool EBDocumentPackage::importDocument(const QString &path,
                                       EBDocument *document, QString *error)
{
    if (!document) {
        if (error)
            *error = QStringLiteral("文档接收对象无效");
        return false;
    }
    const QFileInfo info(path);
    if (!info.isFile() || info.size() <= 0
        || info.size() > kMaxPackageBytes) {
        if (error)
            *error = QStringLiteral("文档包不存在、为空或超过 128 MB");
        return false;
    }

    QFile file(info.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    const QByteArray signature = file.peek(6);
    if (signature.size() == 6
        && qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(
               signature.constData())) == kPackageMagic
        && qFromBigEndian<quint16>(reinterpret_cast<const uchar *>(
               signature.constData() + 4)) == 2) {
        EBDocument imported;
        if (!EBDocumentPackageAssets::importDocument(
                info.absoluteFilePath(), &imported, error)) {
            if (error && error->isEmpty())
                *error = QStringLiteral("文档包格式无效或内容已损坏");
            return false;
        }
        imported._id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        imported._createdAt = QDateTime::currentDateTimeUtc();
        *document = imported;
        return true;
    }
    const QByteArray package = file.readAll();
    if (package.size() <= kHeaderBytes
        || qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(
               package.constData())) != kPackageMagic
        || qFromBigEndian<quint16>(reinterpret_cast<const uchar *>(
               package.constData() + 4)) != kPackageVersion) {
        if (error)
            *error = QStringLiteral("文档包格式或版本无效");
        return false;
    }
    const QByteArray digest = package.mid(4 + sizeof(quint16), kDigestBytes);
    const QByteArray compressed = package.mid(kHeaderBytes);
    if (digest.size() != kDigestBytes
        || compressed.size() < int(sizeof(quint32))
        || compressed.size() > kMaxPackageBytes - kHeaderBytes) {
        if (error)
            *error = QStringLiteral("文档包格式或版本无效");
        return false;
    }

    const quint32 expectedSize = qFromBigEndian<quint32>(
        reinterpret_cast<const uchar *>(compressed.constData()));
    if (expectedSize == 0 || expectedSize > kMaxDocumentBytes) {
        if (error)
            *error = QStringLiteral("文档包声明的内容大小无效");
        return false;
    }
    const QByteArray json = qUncompress(compressed);
    if (quint32(json.size()) != expectedSize
        || QCryptographicHash::hash(json, QCryptographicHash::Sha256) != digest) {
        if (error)
            *error = QStringLiteral("文档包内容不完整或已损坏");
        return false;
    }

    EBDocument imported;
    if (!EBDocumentStorage::fromJson(json, &imported, error))
        return false;
    // 导入视为本机的新副本，避免覆盖原文档或再次导入的副本。
    imported._id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    imported._createdAt = QDateTime::currentDateTimeUtc();
    *document = imported;
    return true;
}
