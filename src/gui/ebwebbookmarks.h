#ifndef EBWEBBOOKMARKS_H
#define EBWEBBOOKMARKS_H

#include <QObject>
#include <QUrl>
#include <QVector>

class EBWebBookmarks : public QObject
{
    Q_OBJECT
public:
    struct Entry {
        QUrl url;
        QString title;
        QString folder;
    };

    explicit EBWebBookmarks(QObject *parent = nullptr);
    const QVector<Entry> &entries() const;
    int indexOf(const QUrl &url) const;
    bool add(const QUrl &url, const QString &title,
             const QString &folder = QString());
    void update(int index, const QString &title, const QString &folder);
    void remove(int index);

signals:
    void changed();

private:
    void save() const;
    QVector<Entry> _entries;
};

#endif
