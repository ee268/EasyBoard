#include "ebpage.h"

qreal EBPage::widthForSize(Size size)
{
    return size == Size::Standard ? 1200.0 : 1600.0;
}

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
    const qreal width = widthForSize(size);
    for (qreal &guide : _verticalGuides)
        guide = qMin(guide, width);
}

const EBPage::Strokes &EBPage::strokes() const
{
    return _strokes;
}

void EBPage::setStrokes(const Strokes &strokes)
{
    _strokes = strokes;
}

const EBPage::Texts &EBPage::texts() const
{
    return _texts;
}

void EBPage::setTexts(const Texts &texts)
{
    _texts = texts;
}

const EBPage::Images &EBPage::images() const
{
    return _images;
}

void EBPage::setImages(const Images &images)
{
    _images = images;
}

const EBPage::TeachingTools &EBPage::teachingTools() const
{
    return _teachingTools;
}

void EBPage::setTeachingTools(const TeachingTools &tools)
{
    _teachingTools = tools;
}

const QVector<qreal> &EBPage::horizontalGuides() const
{
    return _horizontalGuides;
}

void EBPage::setHorizontalGuides(const QVector<qreal> &guides)
{
    _horizontalGuides = guides;
}

const QVector<qreal> &EBPage::verticalGuides() const
{
    return _verticalGuides;
}

void EBPage::setVerticalGuides(const QVector<qreal> &guides)
{
    _verticalGuides = guides;
}

bool EBPage::hasBackgroundImage() const
{
    return !_backgroundImage.isNull();
}

const QImage &EBPage::backgroundImage() const
{
    return _backgroundImage;
}

void EBPage::setBackgroundImage(const QImage &image)
{
    _backgroundImage = image;
}

void EBPage::clearBackgroundImage()
{
    _backgroundImage = QImage();
}
