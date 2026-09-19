#ifndef EBPAGE_H
#define EBPAGE_H

#include <QVector>

#include "../board/ebstrokeitem.h"

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
    using Strokes = QVector<EBStrokeItem::State>;

    Color color() const;
    void setColor(Color color);

    Pattern pattern() const;
    void setPattern(Pattern pattern);

    Size size() const;
    void setSize(Size size);

    const Strokes &strokes() const;
    void setStrokes(const Strokes &strokes);

private:
    Color _color = Color::White;
    Pattern _pattern = Pattern::Blank;
    Size _size = Size::Standard;
    Strokes _strokes;
};

#endif
