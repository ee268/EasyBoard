#include "ebwebhistory.h"

#include <QDir>
#include <QSettings>

#include "../core/ebsettings.h"

namespace {
QString historyPath()
{
    return QDir(EBSettings::userDataDir()).filePath(
        QStringLiteral("web/history.ini"));
}
}

EBWebHistory::EBWebHistory(QObject *parent)
    : QObject(parent)
{
    QSettings settings(historyPath(), QSettings::IniFormat);
    settings.setIniCodec("UTF-8");
    const int count = qMin(settings.beginReadArray(QStringLiteral("Visits")), 500);
    for (int index = 0; index < count; ++index) {
        settings.setArrayIndex(index);
        const QUrl url(settings.value(QStringLiteral("Url")).toString());
        if (!url.isValid() || url.isEmpty())
            continue;
        _entries.append({url,
            settings.value(QStringLiteral("Title")).toString(),
            settings.value(QStringLiteral("Visited")).toDateTime()});
    }
    settings.endArray();
}

const QVector<EBWebHistory::Entry> &EBWebHistory::entries() const
{
    return _entries;
}

void EBWebHistory::record(const QUrl &url, const QString &title)
{
    if (!url.isValid() || (url.scheme() != QStringLiteral("http")
                           && url.scheme() != QStringLiteral("https")
                           && url.scheme() != QStringLiteral("file")))
        return;
    _entries.prepend({url, title.trimmed(), QDateTime::currentDateTime()});
    if (_entries.size() > 500)
        _entries.resize(500);
    save();
    emit changed();
}

void EBWebHistory::clear()
{
    _entries.clear();
    save();
    emit changed();
}

void EBWebHistory::save() const
{
    QDir().mkpath(QDir(EBSettings::userDataDir()).filePath(
        QStringLiteral("web")));
    QSettings settings(historyPath(), QSettings::IniFormat);
    settings.setIniCodec("UTF-8");
    settings.remove(QStringLiteral("Visits"));
    settings.beginWriteArray(QStringLiteral("Visits"));
    for (int index = 0; index < _entries.size(); ++index) {
        settings.setArrayIndex(index);
        const Entry &entry = _entries.at(index);
        settings.setValue(QStringLiteral("Url"), entry.url.toString());
        settings.setValue(QStringLiteral("Title"), entry.title);
        settings.setValue(QStringLiteral("Visited"), entry.visited);
    }
    settings.endArray();
    settings.sync();
}
