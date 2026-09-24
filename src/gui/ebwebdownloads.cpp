#include "ebwebdownloads.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QStandardPaths>
#include <QWebEngineDownloadItem>
#include <QWebEngineProfile>

#include "ebwebdownloadsdialog.h"

EBWebDownloads::EBWebDownloads(QWebEngineProfile *profile, QWidget *window)
    : QObject(window)
    , _window(window)
{
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
        entry.path = file.absoluteFilePath();
        entry.status = tr("正在下载");
        entry.running = true;
        entry.item = download;
        _entries.append(entry);
        emit entryAdded(index);
        connect(download, &QWebEngineDownloadItem::downloadProgress,
                this, [this, index](qint64 received, qint64 total) {
            Entry &entry = _entries[index];
            entry.received = received;
            entry.total = total;
            emit entryChanged(index);
        });
        connect(download, &QWebEngineDownloadItem::finished,
                this, [this, download, index]() {
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
        });
        connect(download, &QObject::destroyed, this, [this, index]() {
            Entry &entry = _entries[index];
            if (entry.running) {
                entry.running = false;
                entry.status = tr("已中断");
                emit entryChanged(index);
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

void EBWebDownloads::showManager()
{
    if (!_dialog) {
        _dialog = new EBWebDownloadsDialog(this, _window);
        _dialog->setAttribute(Qt::WA_DeleteOnClose);
    }
    _dialog->show();
    _dialog->raise();
    _dialog->activateWindow();
}
