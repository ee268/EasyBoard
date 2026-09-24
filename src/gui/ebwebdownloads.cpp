#include "ebwebdownloads.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QSettings>
#include <QStandardPaths>
#include <QWebEngineDownloadItem>
#include <QWebEngineProfile>

#include "ebwebdownloadsdialog.h"
#include "../core/ebsettings.h"

namespace {
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
    connect(profile, &QWebEngineProfile::downloadRequested,
            this, [this](QWebEngineDownloadItem *download) {
        const QString directory = QStandardPaths::writableLocation(
            QStandardPaths::DownloadLocation);
        const QString initial = directory + QLatin1Char('/')
                                + download->suggestedFileName();
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
        entry.status = tr("正在下载");
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
                entry.status = tr("已完成");
                emit statusMessage(tr("下载完成：%1").arg(entry.path));
            } else if (download->state()
                       == QWebEngineDownloadItem::DownloadInterrupted) {
                entry.status = tr("失败：%1")
                    .arg(download->interruptReasonString());
                emit statusMessage(entry.status);
            } else {
                entry.status = tr("已取消");
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
                entry.status = tr("已中断");
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
    entry.status = tr("已完成");
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
        entry.status = settings.value(QStringLiteral("Running")).toBool()
            ? tr("已中断") : settings.value(QStringLiteral("Status")).toString();
        if (entry.completed)
            entry.status = QFileInfo(entry.path).isFile()
                ? tr("已完成") : tr("文件已移动或删除");
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
        settings.setValue(QStringLiteral("Status"), entry.status);
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
        const QString status = QFileInfo(entry.path).isFile()
            ? tr("已完成") : tr("文件已移动或删除");
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
