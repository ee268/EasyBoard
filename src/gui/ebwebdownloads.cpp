#include "ebwebdownloads.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QSettings>
#include <QWebEngineDownloadItem>
#include <QWebEngineProfile>

#include "ebwebdownloadsdialog.h"
#include "../core/ebsettings.h"

namespace {
QString statusCode(EBWebDownloads::Status status)
{
    switch (status) {
    case EBWebDownloads::Status::Downloading: return QStringLiteral("downloading");
    case EBWebDownloads::Status::Completed: return QStringLiteral("completed");
    case EBWebDownloads::Status::Missing: return QStringLiteral("missing");
    case EBWebDownloads::Status::Cancelled: return QStringLiteral("cancelled");
    case EBWebDownloads::Status::Failed: return QStringLiteral("failed");
    case EBWebDownloads::Status::Interrupted: return QStringLiteral("interrupted");
    }
    return QStringLiteral("interrupted");
}

EBWebDownloads::Status parseStatus(const QString &code)
{
    if (code == QStringLiteral("downloading"))
        return EBWebDownloads::Status::Downloading;
    if (code == QStringLiteral("completed"))
        return EBWebDownloads::Status::Completed;
    if (code == QStringLiteral("missing"))
        return EBWebDownloads::Status::Missing;
    if (code == QStringLiteral("cancelled"))
        return EBWebDownloads::Status::Cancelled;
    if (code == QStringLiteral("failed"))
        return EBWebDownloads::Status::Failed;
    return EBWebDownloads::Status::Interrupted;
}

QString downloadsPath()
{
    return QDir(EBSettings::userDataDir()).filePath(
        QStringLiteral("web/downloads.ini"));
}
}

EBWebDownloads::EBWebDownloads(QWebEngineProfile *profile, QWidget *window)
    : QObject(window)
    , _window(window)
{
    load();
    if (!profile)
        return;
    connect(profile, &QWebEngineProfile::downloadRequested,
            this, [this](QWebEngineDownloadItem *download) {
        const QString initial = QDir(EBSettings::settings()->downloadDirectory())
            .filePath(download->suggestedFileName());
        const QString path = QFileDialog::getSaveFileName(
            _window, tr("保存网页下载"), initial);
        if (path.isEmpty()) {
            download->cancel();
            return;
        }
        const QFileInfo file(path);
        download->setDownloadDirectory(file.absolutePath());
        download->setDownloadFileName(file.fileName());
        const int index = _entries.size();
        Entry entry;
        entry.id = _nextId++;
        entry.path = file.absoluteFilePath();
        entry.status = Status::Downloading;
        entry.running = true;
        entry.item = download;
        _entries.append(entry);
        const quint64 id = entry.id;
        emit entryAdded(index);
        save();
        connect(download, &QWebEngineDownloadItem::downloadProgress,
                this, [this, id](qint64 received, qint64 total) {
            const int index = indexFor(id);
            if (index < 0)
                return;
            Entry &entry = _entries[index];
            entry.received = received;
            entry.total = total;
            emit entryChanged(index);
        });
        connect(download, &QWebEngineDownloadItem::finished,
                this, [this, download, id]() {
            const int index = indexFor(id);
            if (index < 0)
                return;
            Entry &entry = _entries[index];
            entry.running = false;
            entry.completed = download->state()
                == QWebEngineDownloadItem::DownloadCompleted;
            if (entry.completed) {
                entry.status = Status::Completed;
                emit statusMessage(tr("下载完成：%1").arg(entry.path));
            } else if (download->state()
                       == QWebEngineDownloadItem::DownloadInterrupted) {
                entry.status = Status::Failed;
                entry.interruptReason = int(download->interruptReason());
                emit statusMessage(statusText(entry));
            } else {
                entry.status = Status::Cancelled;
                emit statusMessage(tr("下载已取消"));
            }
            emit entryChanged(index);
            save();
        });
        connect(download, &QObject::destroyed, this, [this, id]() {
            const int index = indexFor(id);
            if (index < 0)
                return;
            Entry &entry = _entries[index];
            if (entry.running) {
                entry.running = false;
                entry.status = Status::Interrupted;
                emit entryChanged(index);
                save();
            }
        });
        download->accept();
        showManager();
    });
}

int EBWebDownloads::count() const
{
    return _entries.size();
}

const EBWebDownloads::Entry &EBWebDownloads::entryAt(int index) const
{
    return _entries.at(index);
}

QString EBWebDownloads::statusText(const Entry &entry) const
{
    switch (entry.status) {
    case Status::Downloading: return tr("正在下载");
    case Status::Completed: return tr("已完成");
    case Status::Missing: return tr("文件已移动或删除");
    case Status::Cancelled: return tr("已取消");
    case Status::Interrupted: return tr("已中断");
    case Status::Failed: break;
    }
    if (!entry.legacyDetail.isEmpty())
        return tr("失败：%1").arg(entry.legacyDetail);
    QString reason;
    switch (entry.interruptReason) {
    case QWebEngineDownloadItem::FileFailed: reason = tr("文件操作失败"); break;
    case QWebEngineDownloadItem::FileAccessDenied: reason = tr("文件访问被拒绝"); break;
    case QWebEngineDownloadItem::FileNoSpace: reason = tr("磁盘空间不足"); break;
    case QWebEngineDownloadItem::FileNameTooLong: reason = tr("文件名过长"); break;
    case QWebEngineDownloadItem::FileTooLarge: reason = tr("文件过大"); break;
    case QWebEngineDownloadItem::FileVirusInfected: reason = tr("文件被安全软件拦截"); break;
    case QWebEngineDownloadItem::FileTransientError: reason = tr("临时文件错误"); break;
    case QWebEngineDownloadItem::FileBlocked: reason = tr("文件下载被阻止"); break;
    case QWebEngineDownloadItem::FileSecurityCheckFailed: reason = tr("文件安全检查失败"); break;
    case QWebEngineDownloadItem::FileTooShort: reason = tr("文件内容不完整"); break;
    case QWebEngineDownloadItem::FileHashMismatch: reason = tr("文件校验失败"); break;
    case QWebEngineDownloadItem::NetworkFailed: reason = tr("网络连接失败"); break;
    case QWebEngineDownloadItem::NetworkTimeout: reason = tr("网络连接超时"); break;
    case QWebEngineDownloadItem::NetworkDisconnected: reason = tr("网络连接中断"); break;
    case QWebEngineDownloadItem::NetworkServerDown: reason = tr("服务器不可用"); break;
    case QWebEngineDownloadItem::NetworkInvalidRequest: reason = tr("网络请求无效"); break;
    case QWebEngineDownloadItem::ServerFailed: reason = tr("服务器返回错误"); break;
    case QWebEngineDownloadItem::ServerBadContent: reason = tr("服务器内容无效"); break;
    case QWebEngineDownloadItem::ServerUnauthorized: reason = tr("服务器需要身份验证"); break;
    case QWebEngineDownloadItem::ServerCertProblem: reason = tr("服务器证书错误"); break;
    case QWebEngineDownloadItem::ServerForbidden: reason = tr("服务器拒绝访问"); break;
    case QWebEngineDownloadItem::ServerUnreachable: reason = tr("无法连接服务器"); break;
    case QWebEngineDownloadItem::UserCanceled: reason = tr("用户取消"); break;
    default: reason = tr("未知错误"); break;
    }
    return tr("失败：%1").arg(reason);
}

void EBWebDownloads::cancel(int index)
{
    if (index < 0 || index >= _entries.size())
        return;
    const Entry &entry = _entries.at(index);
    if (entry.running && entry.item)
        entry.item->cancel();
}

bool EBWebDownloads::removeRecord(int index)
{
    if (index < 0 || index >= _entries.size() || _entries.at(index).running)
        return false;
    _entries.remove(index);
    save();
    emit entriesReset();
    return true;
}

void EBWebDownloads::clearFinished()
{
    QVector<Entry> active;
    for (const Entry &entry : _entries) {
        if (entry.running)
            active.append(entry);
    }
    if (active.size() == _entries.size())
        return;
    _entries = active;
    save();
    emit entriesReset();
}

bool EBWebDownloads::relink(int index, const QString &path, QString *error)
{
    if (index < 0 || index >= _entries.size()
        || !_entries.at(index).completed)
        return false;
    const QFileInfo file(path);
    if (!file.isFile() || (_entries.at(index).received > 0
        && file.size() != _entries.at(index).received)) {
        if (error)
            *error = tr("请选择原下载文件，文件大小必须一致");
        return false;
    }
    Entry &entry = _entries[index];
    entry.path = file.absoluteFilePath();
    entry.status = Status::Completed;
    save();
    emit entryChanged(index);
    return true;
}

void EBWebDownloads::showManager()
{
    refreshFiles();
    if (!_dialog) {
        _dialog = new EBWebDownloadsDialog(this, _window);
        _dialog->setAttribute(Qt::WA_DeleteOnClose);
    }
    _dialog->show();
    _dialog->raise();
    _dialog->activateWindow();
}

void EBWebDownloads::load()
{
    QSettings settings(downloadsPath(), QSettings::IniFormat);
    settings.setIniCodec("UTF-8");
    const int count = qMin(settings.beginReadArray(QStringLiteral("Downloads")), 200);
    for (int index = 0; index < count; ++index) {
        settings.setArrayIndex(index);
        Entry entry;
        entry.id = _nextId++;
        entry.path = settings.value(QStringLiteral("Path")).toString();
        if (!QFileInfo(entry.path).isAbsolute() || entry.path.size() > 4096)
            continue;
        entry.received = qMax(qint64(0),
            settings.value(QStringLiteral("Received")).toLongLong());
        entry.total = qMax(qint64(0),
            settings.value(QStringLiteral("Total")).toLongLong());
        entry.completed = settings.value(QStringLiteral("Completed")).toBool();
        const QString code = settings.value(QStringLiteral("StatusCode")).toString();
        entry.status = parseStatus(code);
        entry.interruptReason = settings.value(QStringLiteral("InterruptReason")).toInt();
        entry.legacyDetail = settings.value(QStringLiteral("LegacyDetail")).toString();
        if (code.isEmpty()) {
            const QString legacy = settings.value(QStringLiteral("Status")).toString();
            if (legacy.startsWith(QStringLiteral("失败："))
                || legacy.startsWith(QStringLiteral("Failed: "))) {
                entry.status = Status::Failed;
                entry.legacyDetail = legacy.startsWith(QStringLiteral("失败："))
                    ? legacy.mid(QStringLiteral("失败：").size()) : legacy.mid(8);
            } else if (legacy == QStringLiteral("已取消")
                       || legacy == QStringLiteral("Cancelled")) {
                entry.status = Status::Cancelled;
            }
        }
        if (settings.value(QStringLiteral("Running")).toBool())
            entry.status = Status::Interrupted;
        if (entry.completed)
            entry.status = QFileInfo(entry.path).isFile()
                ? Status::Completed : Status::Missing;
        _entries.append(entry);
    }
    settings.endArray();
}

void EBWebDownloads::save() const
{
    QDir().mkpath(QDir(EBSettings::userDataDir()).filePath(
        QStringLiteral("web")));
    QSettings settings(downloadsPath(), QSettings::IniFormat);
    settings.setIniCodec("UTF-8");
    settings.remove(QStringLiteral("Downloads"));
    const int first = qMax(0, _entries.size() - 200);
    settings.beginWriteArray(QStringLiteral("Downloads"));
    for (int index = first; index < _entries.size(); ++index) {
        settings.setArrayIndex(index - first);
        const Entry &entry = _entries.at(index);
        settings.setValue(QStringLiteral("Path"), entry.path);
        settings.setValue(QStringLiteral("StatusCode"), statusCode(entry.status));
        settings.setValue(QStringLiteral("InterruptReason"), entry.interruptReason);
        if (!entry.legacyDetail.isEmpty())
            settings.setValue(QStringLiteral("LegacyDetail"), entry.legacyDetail);
        settings.setValue(QStringLiteral("Received"), entry.received);
        settings.setValue(QStringLiteral("Total"), entry.total);
        settings.setValue(QStringLiteral("Completed"), entry.completed);
        settings.setValue(QStringLiteral("Running"), entry.running);
    }
    settings.endArray();
    settings.sync();
}

void EBWebDownloads::refreshFiles()
{
    for (int index = 0; index < _entries.size(); ++index) {
        Entry &entry = _entries[index];
        if (!entry.completed)
            continue;
        const Status status = QFileInfo(entry.path).isFile()
            ? Status::Completed : Status::Missing;
        if (entry.status != status) {
            entry.status = status;
            emit entryChanged(index);
        }
    }
}

int EBWebDownloads::indexFor(quint64 id) const
{
    for (int index = 0; index < _entries.size(); ++index) {
        if (_entries.at(index).id == id)
            return index;
    }
    return -1;
}
