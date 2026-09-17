#ifndef EBSETTINGS_H
#define EBSETTINGS_H

#include <QString>
#include <QObject>
#include <QPointer>

class QSettings;

class EBSettings : public QObject
{
public:
    static EBSettings* settings();

    static void destroy();

    static QString userDataDir();

    static QString logDir();

    QByteArray windowGeometry() const;

    void setWindowGeometry(const QByteArray& geometry);

    bool save();

private:
    explicit EBSettings(QObject *parent = nullptr);

    static QPointer<EBSettings> _instance;

    QSettings* _userSettings;
};

#endif // EBSETTINGS_H
