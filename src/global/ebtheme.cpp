#include "ebtheme.h"

EBTheme::EBTheme(QObject *parent)
    : QObject{parent}
    , _curThemeType(Light)
{
    initLightColor();
    initDarkColor();
    initPageColors();
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

void EBTheme::setThemeColor(const EBThemeColor &colorName, const QColor &color)
{
    _lightColorList[colorName] = color;
    _darkColorList[colorName] = color;
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
    _lightColorList[BoardBorder] = QColor(0xBE, 0xC3, 0xCA);
    _lightColorList[BoardBackground] = QColor(0xE8, 0xEB, 0xEF);
    _lightColorList[BoardPen] = QColor(0x22, 0x2E, 0x40);
    _lightColorList[BoardMarker] = QColor(0xFF, 0xB8, 0x33, 0x6E);
    _lightColorList[BoardPointerFill] = QColor(0xE1, 0x35, 0x44);
    _lightColorList[BoardPointerBorder] = Qt::white;
    _lightColorList[BoardAlignmentGuide] = QColor(0x22, 0xA9, 0x9B);
    _lightColorList[BoardManualGuide] = QColor(0x38, 0x8F, 0xD4);
    _lightColorList[BoardRulerBackground] = QColor(0xF4, 0xF7, 0xF9);
    _lightColorList[BoardRulerText] = QColor(0x52, 0x65, 0x73);
}

void EBTheme::initPageColors()
{
    // 页面可选底色和底纹线色统一由主题提供，深浅界面使用相同纸张颜色。
    setThemeColor(BoardWhite, QColor(Qt::white));
    setThemeColor(BoardCream, QColor(0xFF, 0xF9, 0xE7));
    setThemeColor(BoardWhitePattern, QColor(0xD1, 0xDE, 0xED));
    setThemeColor(BoardCreamPattern, QColor(0xDE, 0xD3, 0xB7));
}

void EBTheme::initDarkColor()
{
    _darkColorList[BoardBorder] = QColor(0x44,0x49,0x50);
    _darkColorList[BoardBackground] = QColor(0x1E,0x1E,0x1E);
    _darkColorList[BoardPen] = QColor(0x22, 0x2E, 0x40);
    _darkColorList[BoardMarker] = QColor(0xFF, 0xB8, 0x33, 0x6E);
    _darkColorList[BoardPointerFill] = QColor(0xE1, 0x35, 0x44);
    _darkColorList[BoardPointerBorder] = Qt::white;
    _darkColorList[BoardAlignmentGuide] = QColor(0x3D, 0xD1, 0xBD);
    _darkColorList[BoardManualGuide] = QColor(0x65, 0xB5, 0xED);
    _darkColorList[BoardRulerBackground] = QColor(0x2E, 0x36, 0x3D);
    _darkColorList[BoardRulerText] = QColor(0xD0, 0xDA, 0xE1);
}
