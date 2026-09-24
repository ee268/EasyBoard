#ifndef EBWEBHISTORY_H
#define EBWEBHISTORY_H

#include <QDateTime>
#include <QObject>
#include <QUrl>
#include <QVector>

class EBWebHistory : public QObject
{
    Q_OBJECT
public:
    struct Entry {
        QUrl url;
        QString title;
        QDateTime visited;
    };

    explicit EBWebHistory(QObject *parent = nullptr);
    const QVector<Entry> &entries() const;
    void record(const QUrl &url, const QString &title);
    void clear();

signals:
    void changed();

private:
    void save() const;
    QVector<Entry> _entries;
};

#endif
