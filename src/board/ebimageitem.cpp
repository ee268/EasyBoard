#include "ebimageitem.h"

#include <QBuffer>
#include <QImageReader>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionGraphicsItem>
#include <QSvgRenderer>

namespace {
constexpr qint64 kMaxImagePixels = 40000000;
constexpr int kMaxImageDataBytes = 64 * 1024 * 1024;
}

EBImageItem::EBImageItem(const State &state)
    : _format(state.format)
    , _valid(false)
{
    applyState(state);
}

EBImageItem::~EBImageItem() = default;

QRectF EBImageItem::boundingRect() const
{
    return QRectF(QPointF(), _size);
}

void EBImageItem::paint(QPainter *painter,
                        const QStyleOptionGraphicsItem *option, QWidget *)
{
    if (!_valid)
        return;
    painter->setRenderHint(QPainter::SmoothPixmapTransform);
    if (_format == Format::Svg && _svgRenderer)
        _svgRenderer->render(painter, boundingRect());
    else
        painter->drawImage(boundingRect(), _image);

    if (option && (option->state & QStyle::State_Selected)) {
        painter->setBrush(Qt::NoBrush);
        painter->setPen(QPen(QColor(QStringLiteral("#36B9AC")), 1.5,
                             Qt::DashLine));
        painter->drawRect(boundingRect().adjusted(1.0, 1.0, -1.0, -1.0));
    }
}

bool EBImageItem::isValid() const
{
    return _valid && _size.isValid() && !_size.isEmpty();
}

EBImageItem::State EBImageItem::state() const
{
    return {_format, _data, _size, pos(), zValue(), transformOriginPoint(),
            scale(), rotation(), _groupId, _locked};
}

void EBImageItem::applyState(const State &state)
{
    prepareGeometryChange();
    _size = state.size;
    _valid = loadPayload(state.format, state.data)
             && _size.isValid() && !_size.isEmpty();
    setPos(state.position);
    setZValue(state.zValue);
    setTransformOriginPoint(state.transformOrigin);
    setScale(state.scale);
    setRotation(state.rotation);
    _groupId = state.groupId;
    _locked = state.locked;
}

QString EBImageItem::groupId() const
{
    return _groupId;
}

void EBImageItem::setGroupId(const QString &groupId)
{
    _groupId = groupId;
}

bool EBImageItem::isLocked() const
{
    return _locked;
}

void EBImageItem::setLocked(bool locked)
{
    _locked = locked;
}

bool EBImageItem::naturalSize(Format format, const QByteArray &data,
                              QSizeF *size)
{
    if (data.isEmpty() || data.size() > kMaxImageDataBytes)
        return false;
    QSizeF result;
    if (format == Format::Svg) {
        QSvgRenderer renderer(data);
        if (!renderer.isValid())
            return false;
        result = renderer.defaultSize();
        if (!result.isValid() || result.isEmpty())
            result = renderer.viewBoxF().size();
    } else {
        QBuffer buffer;
        buffer.setData(data);
        buffer.open(QIODevice::ReadOnly);
        QImageReader reader(&buffer, "PNG");
        const QSize imageSize = reader.size();
        if (!reader.canRead() || !imageSize.isValid())
            return false;
        result = imageSize;
    }
    if (!result.isValid() || result.isEmpty()
        || qint64(result.width()) * qint64(result.height()) > kMaxImagePixels)
        return false;
    if (size)
        *size = result;
    return true;
}

bool EBImageItem::loadPayload(Format format, const QByteArray &data)
{
    QSizeF sourceSize;
    if (!naturalSize(format, data, &sourceSize))
        return false;
    _format = format;
    _data = data;
    _image = QImage();
    _svgRenderer.reset();
    if (format == Format::Svg) {
        _svgRenderer = std::make_unique<QSvgRenderer>(data);
        return _svgRenderer->isValid();
    }
    _image = QImage::fromData(data, "PNG");
    return !_image.isNull();
}
