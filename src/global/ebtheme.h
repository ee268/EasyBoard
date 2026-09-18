#ifndef EBTHEME_H
#define EBTHEME_H

#include <QObject>
#include <QColor>

#include "ebsingleton.h"

#define ebThemeInst EBTheme::getInstance()

#define ebThemeColor(color) \
    ebThemeInst->getThemeColor(color)

enum EBThemeColor
{
    Board,
    BoardBorder,
    BoardBackground,
    BoardPen,
    BoardMarker,
    BoardPointerFill,
    BoardPointerBorder,
    ColorCount
};

enum EBThemeType
{
    Dark,
    Light
};

class EBTheme : public QObject
            , public EBSingleton<EBTheme>
{
    Q_OBJECT
    friend class EBSingleton<EBTheme>;

public:
    ~EBTheme() = default;

    QColor getThemeColor(const EBThemeType& type, const EBThemeColor& color) const;
    QColor getThemeColor(const EBThemeColor& color) const;

    EBThemeType getThemeType() const;

    void setThemeType(const EBThemeType& type);

private:
    explicit EBTheme(QObject *parent = nullptr);

    void initLightColor();
    void initDarkColor();

private:
    QColor _lightColorList[ColorCount];
    QColor _darkColorList[ColorCount];

    EBThemeType _curThemeType;

signals:
    void onThemeTypeChanged(const EBThemeType& themeType);
};

#endif // EBTHEME_H
