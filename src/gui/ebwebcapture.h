#ifndef EBWEBCAPTURE_H
#define EBWEBCAPTURE_H

#include <QImage>
#include <QPointer>
#include <QWidget>

class QWebEngineView;

// 在网页视图上框选可见区域，截图数据交由主窗口插入白板。
class EBWebCapture : public QWidget
{
    Q_OBJECT
public:
    explicit EBWebCapture(QWebEngineView *view);

signals:
    void captured(const QImage &image);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    QPointer<QWebEngineView> _view;
    QPoint _origin;
    QRect _selection;
    bool _selecting = false;
};

#endif
