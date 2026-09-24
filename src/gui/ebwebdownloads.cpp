#include "ebwebdownloads.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QProgressDialog>
#include <QStandardPaths>
#include <QWebEngineDownloadItem>
#include <QWebEngineProfile>

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
        QProgressDialog *progress = new QProgressDialog(
            tr("正在下载：%1").arg(file.fileName()), tr("取消"), 0, 100,
            _window);
        progress->setObjectName(QStringLiteral("webDownloadProgress"));
        progress->setWindowModality(Qt::NonModal);
        progress->setAutoClose(false);
        progress->setMinimumDuration(0);
        connect(progress, &QProgressDialog::canceled,
                download, &QWebEngineDownloadItem::cancel);
        connect(download, &QWebEngineDownloadItem::downloadProgress,
                progress, [progress](qint64 received, qint64 total) {
            if (total <= 0) {
                progress->setRange(0, 0);
            } else {
                progress->setRange(0, 100);
                progress->setValue(int(qBound(qint64(0),
                    received * 100 / total, qint64(100))));
            }
        });
        connect(download, &QWebEngineDownloadItem::finished,
                this, [this, download, progress, path]() {
            progress->close();
            progress->deleteLater();
            if (download->state() == QWebEngineDownloadItem::DownloadCompleted)
                emit statusMessage(tr("下载完成：%1").arg(path));
            else if (download->state() == QWebEngineDownloadItem::DownloadInterrupted)
                emit statusMessage(tr("下载失败：%1")
                                   .arg(download->interruptReasonString()));
            else
                emit statusMessage(tr("下载已取消"));
        });
        download->accept();
        progress->show();
    });
}
