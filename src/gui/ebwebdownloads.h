#ifndef EBWEBDOWNLOADS_H
#define EBWEBDOWNLOADS_H

#include <QObject>
#include <QPointer>
#include <QString>
#include <QVector>

class QWebEngineDownloadItem;
class QWebEngineProfile;
class QWidget;
class EBWebDownloadsDialog;

class EBWebDownloads : public QObject
{
    Q_OBJECT
public:
    struct Entry {
        QString path;
        QString status;
        qint64 received = 0;
        qint64 total = 0;
        bool running = false;
        bool completed = false;
        QPointer<QWebEngineDownloadItem> item;
    };

    EBWebDownloads(QWebEngineProfile *profile, QWidget *window);
    int count() const;
    const Entry &entryAt(int index) const;
    void cancel(int index);
    void showManager();

signals:
    void statusMessage(const QString &message);
    void entryAdded(int index);
    void entryChanged(int index);

private:
    void load();
    void save() const;
    void refreshFiles();

    QWidget *_window;
    QVector<Entry> _entries;
    QPointer<EBWebDownloadsDialog> _dialog;
};

#endif
