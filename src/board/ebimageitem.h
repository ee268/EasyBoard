#ifndef EBIMAGEITEM_H
#define EBIMAGEITEM_H

#include <QByteArray>
#include <QGraphicsItem>
#include <QImage>
#include <QSizeF>

#include <memory>

class QSvgRenderer;

// 独立图片对象同时支持压缩位图和原始 SVG 数据。
class EBImageItem : public QGraphicsItem
{
public:
    enum class Format {
        Png,
        Svg
    };

    struct State {
        Format format = Format::Png;
        QByteArray data;
        QSizeF size;
        QPointF position;
        qreal zValue = 0.8;
        QPointF transformOrigin;
        qreal scale = 1.0;
        qreal rotation = 0.0;
        QString groupId;
    };

    explicit EBImageItem(const State &state);
    ~EBImageItem() override;

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget = nullptr) override;

    bool isValid() const;
    State state() const;
    void applyState(const State &state);
    QString groupId() const;
    void setGroupId(const QString &groupId);

    static bool naturalSize(Format format, const QByteArray &data,
                            QSizeF *size = nullptr);

private:
    bool loadPayload(Format format, const QByteArray &data);

    Format _format;
    QByteArray _data;
    QSizeF _size;
    QImage _image;
    std::unique_ptr<QSvgRenderer> _svgRenderer;
    bool _valid;
    QString _groupId;
};

#endif
