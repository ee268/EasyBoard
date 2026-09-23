#include "ebsettings.h"

#include <QDir>
#include <QStandardPaths>
#include <QSettings>
#include <QtMath>

QPointer<EBSettings> EBSettings::_instance = nullptr;

EBSettings *EBSettings::settings()
{
    if (!_instance) {
        _instance = new EBSettings();
    }

    return _instance;
}

void EBSettings::destroy()
{
    //_instance由QPointer管理，data取出内部管理的指针并delete
    delete _instance.data();
}

QString EBSettings::userDataDir()
{
    // 测试可指定工作区内的临时目录；正常启动使用 Qt 提供的本机用户数据目录。
    const QString testDirectory = qEnvironmentVariable("EBOARD_DATA_DIR");
    if (!testDirectory.isEmpty())
        return QDir(testDirectory).absolutePath();

    // 隔离数据；原版设置系统未来可在此基础上扩展路径配置。
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    return QDir(base).absoluteFilePath(QStringLiteral("EasyBoard"));
}

QString EBSettings::logDir()
{
    return QDir(userDataDir()).absoluteFilePath(QStringLiteral("log"));
}

QString EBSettings::courseDataDir()
{
    return QDir(userDataDir()).absoluteFilePath(QStringLiteral("data"));
}

QByteArray EBSettings::windowGeometry() const
{
    return _userSettings->value(QStringLiteral("Window/Geometry")).toByteArray();
}

void EBSettings::setWindowGeometry(const QByteArray &geometry)
{
    _userSettings->setValue(QStringLiteral("Window/Geometry"), geometry);
}

QString EBSettings::lastDocumentPath() const
{
    return _userSettings->value(QStringLiteral("Document/LastPath")).toString();
}

void EBSettings::setLastDocumentPath(const QString &path)
{
    _userSettings->setValue(QStringLiteral("Document/LastPath"), path);
}

bool EBSettings::snapEnabled() const
{
    return _userSettings->value(QStringLiteral("Board/SnapEnabled"), true).toBool();
}

void EBSettings::setSnapEnabled(bool enabled)
{
    _userSettings->setValue(QStringLiteral("Board/SnapEnabled"), enabled);
}

bool EBSettings::gridSnapEnabled() const
{
    return _userSettings->value(QStringLiteral("Board/GridSnapEnabled"), false).toBool();
}

void EBSettings::setGridSnapEnabled(bool enabled)
{
    _userSettings->setValue(QStringLiteral("Board/GridSnapEnabled"), enabled);
}

QColor EBSettings::penColor() const
{
    const QColor color = _userSettings->value(QStringLiteral("Brush/PenColor"),
        QColor(0x22, 0x2E, 0x40)).value<QColor>();
    return color.isValid() ? color : QColor(0x22, 0x2E, 0x40);
}

void EBSettings::setPenColor(const QColor &color)
{
    _userSettings->setValue(QStringLiteral("Brush/PenColor"), color);
}

QColor EBSettings::markerColor() const
{
    const QColor color = _userSettings->value(QStringLiteral("Brush/MarkerColor"),
        QColor(0xFF, 0xB8, 0x33, 0x6E)).value<QColor>();
    return color.isValid() ? color : QColor(0xFF, 0xB8, 0x33, 0x6E);
}

void EBSettings::setMarkerColor(const QColor &color)
{
    _userSettings->setValue(QStringLiteral("Brush/MarkerColor"), color);
}

qreal EBSettings::penWidth() const
{
    const qreal width = _userSettings->value(
        QStringLiteral("Brush/PenWidth"), 3.0).toDouble();
    return qIsFinite(width) ? qBound(1.0, width, 24.0) : 3.0;
}

void EBSettings::setPenWidth(qreal width)
{
    _userSettings->setValue(QStringLiteral("Brush/PenWidth"), width);
}

qreal EBSettings::markerWidth() const
{
    const qreal width = _userSettings->value(
        QStringLiteral("Brush/MarkerWidth"), 18.0).toDouble();
    return qIsFinite(width) ? qBound(4.0, width, 48.0) : 18.0;
}

void EBSettings::setMarkerWidth(qreal width)
{
    _userSettings->setValue(QStringLiteral("Brush/MarkerWidth"), width);
}

bool EBSettings::save()
{
    //同步保存到配置文件中
    _userSettings->sync();

    //查看保存状态是否无错误
    return _userSettings->status() == QSettings::NoError;
}

EBSettings::EBSettings(QObject *parent)
    : QObject(parent)
    , _userSettings(nullptr)
{
    QDir().mkpath(userDataDir());
    const QString filePath = QDir(userDataDir()).absoluteFilePath(
        QStringLiteral("EasyBoard.ini"));

    _userSettings = new QSettings(filePath, QSettings::IniFormat, this);
    _userSettings->setIniCodec("UTF-8");
}

