#include "ebwebbookmarks.h"

#include <QDir>
#include <QSettings>

#include "../core/ebsettings.h"

namespace {
QString bookmarksPath()
{
    return QDir(EBSettings::userDataDir()).filePath(
        QStringLiteral("web/bookmarks.ini"));
}

bool canBookmark(const QUrl &url)
{
    return url.isValid() && (url.scheme() == QStringLiteral("http")
        || url.scheme() == QStringLiteral("https")
        || url.scheme() == QStringLiteral("file"));
}
}

EBWebBookmarks::EBWebBookmarks(QObject *parent)
    : QObject(parent)
{
    QSettings settings(bookmarksPath(), QSettings::IniFormat);
    settings.setIniCodec("UTF-8");
    const int count = qMin(settings.beginReadArray(QStringLiteral("Bookmarks")), 500);
    for (int index = 0; index < count; ++index) {
        settings.setArrayIndex(index);
        const QUrl url(settings.value(QStringLiteral("Url")).toString());
        if (!canBookmark(url) || indexOf(url) >= 0)
            continue;
        _entries.append({url, settings.value(QStringLiteral("Title")).toString(),
                         settings.value(QStringLiteral("Folder")).toString()});
    }
    settings.endArray();
}

const QVector<EBWebBookmarks::Entry> &EBWebBookmarks::entries() const
{
    return _entries;
}

int EBWebBookmarks::indexOf(const QUrl &url) const
{
    for (int index = 0; index < _entries.size(); ++index) {
        if (_entries.at(index).url == url)
            return index;
    }
    return -1;
}

bool EBWebBookmarks::add(const QUrl &url, const QString &title,
                         const QString &folder)
{
    if (!canBookmark(url))
        return false;
    const int index = indexOf(url);
    if (index >= 0) {
        update(index, title.isEmpty() ? _entries.at(index).title : title,
               folder.isEmpty() ? _entries.at(index).folder : folder);
        return true;
    }
    if (_entries.size() >= 500)
        return false;
    _entries.prepend({url, title.trimmed(), folder.trimmed()});
    save();
    emit changed();
    return true;
}

void EBWebBookmarks::update(int index, const QString &title,
                            const QString &folder)
{
    if (index < 0 || index >= _entries.size())
        return;
    Entry &entry = _entries[index];
    entry.title = title.trimmed();
    entry.folder = folder.trimmed();
    save();
    emit changed();
}

void EBWebBookmarks::remove(int index)
{
    if (index < 0 || index >= _entries.size())
        return;
    _entries.removeAt(index);
    save();
    emit changed();
}

void EBWebBookmarks::save() const
{
    QDir().mkpath(QDir(EBSettings::userDataDir()).filePath(
        QStringLiteral("web")));
    QSettings settings(bookmarksPath(), QSettings::IniFormat);
    settings.setIniCodec("UTF-8");
    settings.remove(QStringLiteral("Bookmarks"));
    settings.beginWriteArray(QStringLiteral("Bookmarks"));
    for (int index = 0; index < _entries.size(); ++index) {
        settings.setArrayIndex(index);
        const Entry &entry = _entries.at(index);
        settings.setValue(QStringLiteral("Url"), entry.url.toString());
        settings.setValue(QStringLiteral("Title"), entry.title);
        settings.setValue(QStringLiteral("Folder"), entry.folder);
    }
    settings.endArray();
    settings.sync();
}
