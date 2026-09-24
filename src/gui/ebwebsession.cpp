#include "ebwebsession.h"

#include <QDir>
#include <QSettings>

#include "../core/ebsettings.h"

namespace {
QString sessionPath()
{
    return QDir(EBSettings::userDataDir()).filePath(
        QStringLiteral("web/session.ini"));
}
}

EBWebSession EBWebSession::load()
{
    QSettings settings(sessionPath(), QSettings::IniFormat);
    settings.setIniCodec("UTF-8");
    EBWebSession session;
    session.addresses = settings.value(QStringLiteral("Tabs/Addresses"))
                            .toStringList();
    session.currentIndex = settings.value(QStringLiteral("Tabs/Current"), 0)
                               .toInt();
    return session;
}

void EBWebSession::save() const
{
    QDir().mkpath(QDir(EBSettings::userDataDir()).filePath(
        QStringLiteral("web")));
    QSettings settings(sessionPath(), QSettings::IniFormat);
    settings.setIniCodec("UTF-8");
    settings.setValue(QStringLiteral("Tabs/Addresses"), addresses);
    settings.setValue(QStringLiteral("Tabs/Current"), currentIndex);
    settings.sync();
}
