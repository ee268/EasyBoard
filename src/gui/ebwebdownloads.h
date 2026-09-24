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
        quint64 id = 0;
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
    bool removeRecord(int index);
    void clearFinished();
    bool relink(int index, const QString &path, QString *error = nullptr);
    void refreshFiles();
    void showManager();

signals:
    void statusMessage(const QString &message);
    void entryAdded(int index);
    void entryChanged(int index);
    void entriesReset();

private:
    void load();
    void save() const;
    int indexFor(quint64 id) const;

    QWidget *_window;
    QVector<Entry> _entries;
    quint64 _nextId = 1;
    QPointer<EBWebDownloadsDialog> _dialog;
};

#endif
