#include "eblanguage.h"

#include <QApplication>
#include <QDir>
#include <QDebug>

EBLanguage *EBLanguage::_instance = nullptr;

EBLanguage::EBLanguage(QObject *parent)
    : QObject(parent)
    , _language(QStringLiteral("zh_CN"))
{
    _instance = this;
    if (_qtTranslator.load(QStringLiteral(":/translations/qt_zh_CN.qm")))
        qApp->installTranslator(&_qtTranslator);
}

EBLanguage *EBLanguage::instance()
{
    return _instance;
}

QString EBLanguage::language() const
{
    return _language;
}

bool EBLanguage::setLanguage(const QString &language)
{
    if (language != QStringLiteral("zh_CN")
        && language != QStringLiteral("en_US"))
        return false;
    if (language == _language)
        return true;
    if (language == QStringLiteral("en_US")) {
        const QString path = QDir(qApp->applicationDirPath()).filePath(
            QStringLiteral("easyboard_en.qm"));
        if (!_englishTranslator.load(path)) {
            qWarning() << "Could not load English translation:" << path;
            return false;
        }
        qApp->removeTranslator(&_qtTranslator);
        qApp->installTranslator(&_englishTranslator);
    } else {
        qApp->removeTranslator(&_englishTranslator);
        qApp->installTranslator(&_qtTranslator);
    }
    _language = language;
    emit languageChanged();
    return true;
}
