#include "ebpdfimporter.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTemporaryDir>

#include "../domain/ebdocument.h"

namespace {
constexpr qint64 kMaxPdfBytes = 256 * 1024 * 1024;
constexpr qint64 kMaxImageBytes = 64 * 1024 * 1024;
constexpr qint64 kMaxImagePixels = 40000000;
}

bool EBPDFImporter::importFile(const QString &path, EBDocument *document,
                               QString *error,
                               const std::function<void(int, int)> &progress,
                               const std::atomic_bool *cancelled)
{
    const QFileInfo file(path);
    if (!document || !file.isFile() || file.size() <= 0
        || file.size() > kMaxPdfBytes) {
        if (error)
            *error = QStringLiteral("PDF 文件不存在、为空或超过 256 MB");
        return false;
    }
    QTemporaryDir temporary;
    if (!temporary.isValid()) {
        if (error)
            *error = QStringLiteral("无法创建 PDF 导入临时目录");
        return false;
    }
    const QString renderer = QDir(QCoreApplication::applicationDirPath()).filePath(
        QStringLiteral("EasyBoardPdfRenderer.exe"));
    if (!QFileInfo::exists(renderer)) {
        if (error)
            *error = QStringLiteral("缺少 PDF 渲染组件 EasyBoardPdfRenderer.exe");
        return false;
    }
    QProcess process;
    process.start(renderer, {file.absoluteFilePath(), temporary.path()});
    if (!process.waitForStarted(10000)) {
        if (error)
            *error = QStringLiteral("无法启动 PDF 渲染组件");
        return false;
    }
    QElapsedTimer elapsed;
    elapsed.start();
    QByteArray messages;
    int total = 0;
    const auto readProgress = [&]() {
        messages += process.readAllStandardOutput();
        int end = messages.indexOf('\n');
        while (end >= 0) {
            const QByteArray line = messages.left(end).trimmed();
            messages.remove(0, end + 1);
            if (line.startsWith("TOTAL ")) {
                total = line.mid(6).toInt();
                if (progress && total > 0)
                    progress(0, total);
            } else if (line.startsWith("PAGE ") && progress && total > 0) {
                progress(line.mid(5).toInt(), total);
            }
            end = messages.indexOf('\n');
        }
    };
    while (process.state() != QProcess::NotRunning) {
        if (cancelled && cancelled->load()) {
            process.kill();
            process.waitForFinished(3000);
            if (error)
                *error = QStringLiteral("已取消 PDF 导入");
            return false;
        }
        if (elapsed.elapsed() > 300000) {
            process.kill();
            process.waitForFinished(3000);
            if (error)
                *error = QStringLiteral("PDF 导入超时");
            return false;
        }
        process.waitForReadyRead(100);
        readProgress();
    }
    readProgress();
    if (cancelled && cancelled->load()) {
        if (error)
            *error = QStringLiteral("已取消 PDF 导入");
        return false;
    }
    if (process.exitStatus() != QProcess::NormalExit
        || process.exitCode() != 0) {
        if (error) {
            const QString detail = QString::fromUtf8(
                process.readAllStandardError()).trimmed();
            *error = detail.isEmpty() ? QStringLiteral("PDF 渲染失败或超时")
                                      : detail;
        }
        return false;
    }
    QFile manifest(QDir(temporary.path()).filePath(QStringLiteral("pages.json")));
    if (!manifest.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("PDF 页面索引缺失");
        return false;
    }
    const QJsonDocument index = QJsonDocument::fromJson(manifest.readAll());
    const QJsonArray pages = index.object().value(QStringLiteral("pages")).toArray();
    if (pages.isEmpty() || pages.size() > 200) {
        if (error)
            *error = QStringLiteral("PDF 页面索引无效");
        return false;
    }
    EBDocument imported;
    imported.setTitle(file.completeBaseName());
    for (int pageIndex = 0; pageIndex < pages.size(); ++pageIndex) {
        if (cancelled && cancelled->load()) {
            if (error)
                *error = QStringLiteral("已取消 PDF 导入");
            return false;
        }
        const QJsonObject pageInfo = pages.at(pageIndex).toObject();
        const QString name = pageInfo.value(QStringLiteral("file")).toString();
        const QJsonValue width = pageInfo.value(QStringLiteral("width"));
        const QString expected = QStringLiteral("page-%1.png").arg(pageIndex + 1);
        const QFileInfo imageFile(QDir(temporary.path()).filePath(name));
        if (name != expected || !width.isDouble() || !imageFile.isFile()
            || imageFile.size() <= 0 || imageFile.size() > kMaxImageBytes) {
            if (error)
                *error = QStringLiteral("第 %1 页图像无效").arg(pageIndex + 1);
            return false;
        }
        QImageReader reader(imageFile.absoluteFilePath(), "PNG");
        const QSize size = reader.size();
        if (!size.isValid()
            || qint64(size.width()) * size.height() > kMaxImagePixels) {
            if (error)
                *error = QStringLiteral("第 %1 页图像过大").arg(pageIndex + 1);
            return false;
        }
        const QImage image = reader.read();
        if (image.isNull()) {
            if (error)
                *error = QStringLiteral("第 %1 页图像读取失败").arg(pageIndex + 1);
            return false;
        }
        if (pageIndex > 0) {
            imported.addPage();
            imported.setCurrentPageIndex(pageIndex);
        }
        EBPage *page = imported.currentPage();
        if (!page->setCustomWidth(width.toDouble())) {
            if (error)
                *error = QStringLiteral("第 %1 页比例无效").arg(pageIndex + 1);
            return false;
        }
        page->setBackgroundImage(image);
    }
    imported.setCurrentPageIndex(0);
    *document = imported;
    return true;
}
