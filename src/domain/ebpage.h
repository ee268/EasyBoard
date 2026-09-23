#ifndef EBPAGE_H
#define EBPAGE_H

#include <QImage>
#include <QVector>

#include "../board/ebstrokeitem.h"
#include "../board/ebtextitem.h"
#include "../board/ebimageitem.h"

// 页面只保存可恢复的内容状态，图元的显示和编辑仍由画板场景负责。
class EBPage
{
public:
    enum class Color {
        White,
        Cream
    };
    enum class Pattern {
        Blank,
        Grid,
        Ruled
    };
    enum class Size {
        Standard,
        Widescreen
    };
    static constexpr qreal Height = 900.0;
    static qreal widthForSize(Size size);
    using Strokes = QVector<EBStrokeItem::State>;
    using Texts = QVector<EBTextItem::State>;
    using Images = QVector<EBImageItem::State>;

    Color color() const;
    void setColor(Color color);

    Pattern pattern() const;
    void setPattern(Pattern pattern);

    Size size() const;
    void setSize(Size size);

    const Strokes &strokes() const;
    void setStrokes(const Strokes &strokes);
    const Texts &texts() const;
    void setTexts(const Texts &texts);
    const Images &images() const;
    void setImages(const Images &images);
    const QVector<qreal> &horizontalGuides() const;
    void setHorizontalGuides(const QVector<qreal> &guides);
    const QVector<qreal> &verticalGuides() const;
    void setVerticalGuides(const QVector<qreal> &guides);

    bool hasBackgroundImage() const;
    const QImage &backgroundImage() const;
    void setBackgroundImage(const QImage &image);
    void clearBackgroundImage();

private:
    Color _color = Color::White;
    Pattern _pattern = Pattern::Blank;
    Size _size = Size::Standard;
    Strokes _strokes;
    Texts _texts;
    Images _images;
    QVector<qreal> _horizontalGuides;
    QVector<qreal> _verticalGuides;
    QImage _backgroundImage;
};

#endif
