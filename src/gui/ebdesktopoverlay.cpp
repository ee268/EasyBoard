#include "ebdesktopoverlay.h"

#include <QGuiApplication>
#include <QKeyEvent>
#include <QLineF>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QScreen>

#include "ebdesktopbar.h"

EBDesktopOverlay::EBDesktopOverlay(QWidget *parent)
    : QWidget(parent)
    , _bar(new EBDesktopBar(this))
{
    setObjectName(QStringLiteral("desktopOverlay"));
    setWindowTitle(tr("EasyBoard 桌面批注"));
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint
                   | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    connect(_bar, &EBDesktopBar::toolSelected,
            this, &EBDesktopOverlay::setTool);
    connect(_bar, &EBDesktopBar::undoRequested,
            this, &EBDesktopOverlay::undo);
    connect(_bar, &EBDesktopBar::redoRequested,
            this, &EBDesktopOverlay::redo);
    connect(_bar, &EBDesktopBar::exitRequested,
            this, &EBDesktopOverlay::exitRequested);
    connect(this, &EBDesktopOverlay::historyAvailabilityChanged,
            _bar, &EBDesktopBar::setHistory);
}

void EBDesktopOverlay::openOnDesktop()
{
    if (QScreen *screen = QGuiApplication::primaryScreen())
        setGeometry(screen->geometry());
    showFullScreen();
    _bar->adjustSize();
    _bar->move((width() - _bar->width()) / 2, 12);
    _bar->raise();
    raise();
    activateWindow();
    setFocus();
}

void EBDesktopOverlay::setBrushes(const QColor &penColor, qreal penWidth,
                                  const QColor &markerColor, qreal markerWidth)
{
    if (penColor.isValid())
        _penColor = penColor;
    if (markerColor.isValid())
        _markerColor = markerColor;
    _penWidth = qBound(1.0, penWidth, 24.0);
    _markerWidth = qBound(4.0, markerWidth, 48.0);
}

void EBDesktopOverlay::setTool(Tool tool)
{
    if (_drawing)
        finishStroke();
    _tool = tool;
}

EBDesktopOverlay::Tool EBDesktopOverlay::tool() const
{
    return _tool;
}

int EBDesktopOverlay::strokeCount() const
{
    return _applied;
}

bool EBDesktopOverlay::canUndo() const
{
    return _applied > 0;
}

bool EBDesktopOverlay::canRedo() const
{
    return _applied < _strokes.size();
}

void EBDesktopOverlay::undo()
{
    if (!canUndo())
        return;
    --_applied;
    update();
    refreshHistory();
}

void EBDesktopOverlay::redo()
{
    if (!canRedo())
        return;
    ++_applied;
    update();
    refreshHistory();
}

void EBDesktopOverlay::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.fillRect(rect(), Qt::transparent);
    painter.setRenderHint(QPainter::Antialiasing);
    for (int index = 0; index < _applied; ++index)
        paintStroke(&painter, _strokes.at(index));
    if (_drawing)
        paintStroke(&painter, _current);
}

void EBDesktopOverlay::paintStroke(QPainter *painter,
                                   const Stroke &stroke) const
{
    painter->setCompositionMode(stroke.tool == Tool::Eraser
        ? QPainter::CompositionMode_Clear : QPainter::CompositionMode_SourceOver);
    QPen pen(stroke.tool == Tool::Eraser ? Qt::black : stroke.color,
             stroke.width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(stroke.path);
}

void EBDesktopOverlay::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    _bar->adjustSize();
    _bar->move((width() - _bar->width()) / 2, 12);
}

void EBDesktopOverlay::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;
    _current = Stroke{_tool, QPainterPath(),
                      _tool == Tool::Marker ? _markerColor : _penColor,
                      _tool == Tool::Eraser ? 32.0
                          : _tool == Tool::Marker ? _markerWidth : _penWidth};
    _current.path.moveTo(event->pos());
    _drawing = true;
    event->accept();
}

void EBDesktopOverlay::mouseMoveEvent(QMouseEvent *event)
{
    if (!_drawing)
        return;
    if (QLineF(_current.path.currentPosition(), event->pos()).length() >= 1.0) {
        _current.path.lineTo(event->pos());
        update();
    }
    event->accept();
}

void EBDesktopOverlay::mouseReleaseEvent(QMouseEvent *event)
{
    if (!_drawing || event->button() != Qt::LeftButton)
        return;
    if (QLineF(_current.path.currentPosition(), event->pos()).length() >= 1.0)
        _current.path.lineTo(event->pos());
    finishStroke();
    event->accept();
}

void EBDesktopOverlay::finishStroke()
{
    if (!_drawing)
        return;
    if (_current.path.elementCount() == 1)
        _current.path.lineTo(_current.path.currentPosition()
                             + QPointF(0.1, 0.1));
    _strokes.resize(_applied);
    _strokes.append(_current);
    ++_applied;
    _drawing = false;
    update();
    refreshHistory();
}

void EBDesktopOverlay::refreshHistory()
{
    emit historyAvailabilityChanged(canUndo(), canRedo());
}

void EBDesktopOverlay::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        emit exitRequested();
    } else if (event->modifiers() & Qt::ControlModifier) {
        if (event->key() == Qt::Key_Z
            && !(event->modifiers() & Qt::ShiftModifier))
            undo();
        else if (event->key() == Qt::Key_Y
                 || (event->key() == Qt::Key_Z
                     && (event->modifiers() & Qt::ShiftModifier)))
            redo();
        else {
            QWidget::keyPressEvent(event);
            return;
        }
    } else {
        QWidget::keyPressEvent(event);
        return;
    }
    event->accept();
}
