#include "ebwebcapture.h"

#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QTimer>
#include <QWebEngineView>
#include <QtMath>

#include "ebscreencapture.h"

EBWebCapture::EBWebCapture(QWebEngineView *view)
    : QWidget(view)
    , _view(view)
{
    setObjectName(QStringLiteral("webCaptureOverlay"));
    setAttribute(Qt::WA_TranslucentBackground);
    setFocusPolicy(Qt::StrongFocus);
    setGeometry(view->rect());
    view->installEventFilter(this);
    show();
    raise();
    setFocus();
}

bool EBWebCapture::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == _view && event->type() == QEvent::Resize)
        setGeometry(_view->rect());
    return QWidget::eventFilter(watched, event);
}

void EBWebCapture::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    const QColor shade(18, 31, 42, 90);
    if (_selection.isEmpty()) {
        painter.fillRect(rect(), shade);
    } else {
        painter.fillRect(QRect(0, 0, width(), _selection.top()), shade);
        painter.fillRect(QRect(0, _selection.top(), _selection.left(),
                               _selection.height()), shade);
        painter.fillRect(QRect(_selection.right() + 1, _selection.top(),
                               width() - _selection.right() - 1,
                               _selection.height()), shade);
        painter.fillRect(QRect(0, _selection.bottom() + 1, width(),
                               height() - _selection.bottom() - 1), shade);
        painter.setPen(QPen(QColor(28, 129, 185), 2.0, Qt::DashLine));
        painter.drawRect(_selection.adjusted(0, 0, -1, -1));
    }
    painter.setPen(Qt::white);
    painter.fillRect(QRect(16, 16, 238, 30), QColor(30, 43, 51, 205));
    painter.drawText(QRect(16, 16, 238, 30), Qt::AlignCenter,
                     tr("拖动框选网页区域，Esc 取消"));
}

void EBWebCapture::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;
    _origin = event->pos();
    _selection = QRect(_origin, QSize());
    _selecting = true;
    update();
}

void EBWebCapture::mouseMoveEvent(QMouseEvent *event)
{
    if (!_selecting)
        return;
    _selection = QRect(_origin, event->pos()).normalized().intersected(rect());
    update();
}

void EBWebCapture::mouseReleaseEvent(QMouseEvent *event)
{
    if (!_selecting || event->button() != Qt::LeftButton)
        return;
    _selecting = false;
    const QRect selected = QRect(_origin, event->pos()).normalized()
                               .intersected(rect());
    hide();
    if (selected.width() < 16 || selected.height() < 16) {
        deleteLater();
        return;
    }
    QTimer::singleShot(160, this, [this, selected]() {
        if (_view && _view->isVisible()) {
            const QPixmap content = _view->grab();
            const qreal ratio = content.devicePixelRatioF();
            const QRect pixels(qRound(selected.x() * ratio),
                               qRound(selected.y() * ratio),
                               qRound(selected.width() * ratio),
                               qRound(selected.height() * ratio));
            QImage image = content.toImage().copy(
                pixels.intersected(content.rect()));
            if (image.isNull()) {
                const QRect global(_view->mapToGlobal(selected.topLeft()),
                                   selected.size());
                image = ebCaptureScreens(global, _view->devicePixelRatioF());
            } else {
                image.setDevicePixelRatio(ratio);
            }
            emit captured(image);
        }
        deleteLater();
    });
}

void EBWebCapture::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        hide();
        deleteLater();
        return;
    }
    QWidget::keyPressEvent(event);
}
