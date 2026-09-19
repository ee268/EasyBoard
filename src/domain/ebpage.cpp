#include "ebpage.h"

EBPage::Color EBPage::color() const
{
    return _color;
}

void EBPage::setColor(Color color)
{
    _color = color;
}

EBPage::Pattern EBPage::pattern() const
{
    return _pattern;
}

void EBPage::setPattern(Pattern pattern)
{
    _pattern = pattern;
}

EBPage::Size EBPage::size() const
{
    return _size;
}

void EBPage::setSize(Size size)
{
    _size = size;
}

const EBPage::Strokes &EBPage::strokes() const
{
    return _strokes;
}

void EBPage::setStrokes(const Strokes &strokes)
{
    _strokes = strokes;
}
