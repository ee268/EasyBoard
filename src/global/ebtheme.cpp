#include "ebtheme.h"

EBTheme::EBTheme(QObject *parent)
    : QObject{parent}
    , _curThemeType(Light)
{
    initLightColor();
    initDarkColor();
}

EBThemeType EBTheme::getThemeType() const
{
    return _curThemeType;
}

void EBTheme::setThemeType(const EBThemeType &type)
{
    _curThemeType = type;
    emit onThemeTypeChanged(_curThemeType);
}

QColor EBTheme::getThemeColor(const EBThemeType &type, const EBThemeColor &color) const
{
    if (type == Light) {
        return _lightColorList[color];
    }
    else if (type == Dark) {
        return _darkColorList[color];
    }

    return QColor();
}

QColor EBTheme::getThemeColor(const EBThemeColor &color) const
{
    if (_curThemeType == Light) {
        return _lightColorList[color];
    }
    else if (_curThemeType == Dark) {
        return _darkColorList[color];
    }

    return QColor();
}

void EBTheme::initLightColor()
{
    _lightColorList[Board] = Qt::white;
    _lightColorList[BoardBorder] = QColor(0xBE, 0xC3, 0xCA);
    _lightColorList[BoardBackground] = QColor(0xE8, 0xEB, 0xEF);
    _lightColorList[BoardPen] = QColor(0x22, 0x2E, 0x40);
    _lightColorList[BoardMarker] = QColor(0xFF, 0xB8, 0x33, 0x6E);
    _lightColorList[BoardPointerFill] = QColor(225, 53, 68);
    _lightColorList[BoardPointerBorder] = Qt::white;
}

void EBTheme::initDarkColor()
{
    _darkColorList[Board] = QColor(0x2B,0x2B,0x2B);
    _darkColorList[BoardBorder] = QColor(0x44,0x49,0x50);
    _darkColorList[BoardBackground] = QColor(0x1E,0x1E,0x1E);
    _darkColorList[BoardPen] = QColor(0x22, 0x2E, 0x40);
    _darkColorList[BoardMarker] = QColor(0xFF, 0xB8, 0x33, 0x6E);
    _darkColorList[BoardPointerFill] = QColor(225, 53, 68);
    _darkColorList[BoardPointerBorder] = Qt::white;
}
