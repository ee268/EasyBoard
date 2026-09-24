#include "ebdesktopoverlay.h"

#include <QGuiApplication>
#include <QHideEvent>
#include <QFileDialog>
#include <QImageWriter>
#include <QKeyEvent>
#include <QLineF>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QScreen>
#include <QSaveFile>
#include <QTimer>
#include <QTransform>
#include <QWindow>

#include "ebdesktopbar.h"
#include "ebscreencapture.h"

#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
#endif

EBDesktopOverlay::EBDesktopOverlay(QWidget *parent)
    : QWidget(parent)
    , _bar(new EBDesktopBar(this))
    , _saveTimer(new QTimer(this))
{
    _savedInk = EBDesktopInkStore::load();
    _saveTimer->setSingleShot(true);
    _saveTimer->setInterval(500);
    connect(_saveTimer, &QTimer::timeout, this, [this]() {
        if (!EBDesktopInkStore::save(_savedInk))
            emit statusMessage(tr("桌面批注保存失败"));
    });
    setObjectName(QStringLiteral("desktopOverlay"));
    setWindowTitle(tr("EasyBoard 桌面批注"));
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint
                   | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    _bar->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint
                         | Qt::WindowStaysOnTopHint);
    connect(_bar, &EBDesktopBar::toolSelected,
            this, &EBDesktopOverlay::setTool);
    connect(_bar, &EBDesktopBar::undoRequested,
            this, &EBDesktopOverlay::undo);
    connect(_bar, &EBDesktopBar::redoRequested,
            this, &EBDesktopOverlay::redo);
    connect(_bar, &EBDesktopBar::clearRequested,
            this, &EBDesktopOverlay::clear);
    connect(_bar, &EBDesktopBar::captureRequested,
            this, &EBDesktopOverlay::captureToBoard);
    connect(_bar, &EBDesktopBar::saveImageRequested,
            this, &EBDesktopOverlay::saveSnapshot);
    connect(_bar, &EBDesktopBar::interactionModeChanged,
            this, &EBDesktopOverlay::setInteractionMode);
    connect(_bar, &EBDesktopBar::screenSelected,
            this, &EBDesktopOverlay::selectScreen);
    connect(_bar, &EBDesktopBar::penColorChanged,
            this, [this](const QColor &color) { _penColor = color; });
    connect(_bar, &EBDesktopBar::markerColorChanged,
            this, [this](const QColor &color) { _markerColor = color; });
    connect(_bar, &EBDesktopBar::penWidthChanged,
            this, [this](qreal width) { _penWidth = width; });
    connect(_bar, &EBDesktopBar::markerWidthChanged,
            this, [this](qreal width) { _markerWidth = width; });
    connect(_bar, &EBDesktopBar::exitRequested,
            this, &EBDesktopOverlay::exitRequested);
    connect(this, &EBDesktopOverlay::historyAvailabilityChanged,
            _bar, &EBDesktopBar::setHistory);
    connect(qGuiApp, &QGuiApplication::primaryScreenChanged,
            this, &EBDesktopOverlay::followScreen);
    connect(qGuiApp, &QGuiApplication::screenRemoved, this,
            [this](QScreen *screen) {
        if (_activeScreen == screen)
            saveScreenInk();
        _screenInk.remove(screen);
        if (_activeScreen == screen) {
            _activeScreen = nullptr;
            _strokes.clear();
            _applied = 0;
            _drawing = false;
            refreshHistory();
        }
        if (_targetScreen == screen)
            _targetScreen = nullptr;
        QTimer::singleShot(0, this, &EBDesktopOverlay::followScreen);
    });
    const auto watchScreen = [this](QScreen *screen) {
        connect(screen, &QScreen::geometryChanged,
                this, &EBDesktopOverlay::followScreen);
    };
    for (QScreen *screen : QGuiApplication::screens())
        watchScreen(screen);
    connect(qGuiApp, &QGuiApplication::screenAdded, this, watchScreen);
}

EBDesktopOverlay::~EBDesktopOverlay()
{
    saveScreenInk();
    EBDesktopInkStore::save(_savedInk);
}

void EBDesktopOverlay::openOnDesktop()
{
    followScreen();
    showFullScreen();
    raise();
    _bar->show();
    _bar->adjustSize();
    positionBar(false);
    _bar->raise();
    setInteractionMode(_interactionMode);
    if (!_interactionMode) {
        activateWindow();
        setFocus();
    }
}

void EBDesktopOverlay::setInteractionMode(bool enabled)
{
    if (_drawing)
        finishStroke();
    const bool changed = _interactionMode != enabled;
    _interactionMode = enabled;
    _bar->setInteractionMode(enabled);
    if (!isVisible())
        return;
#ifdef Q_OS_WIN
    const HWND handle = reinterpret_cast<HWND>(winId());
    LONG_PTR style = GetWindowLongPtrW(handle, GWL_EXSTYLE);
    style = enabled ? style | WS_EX_TRANSPARENT : style & ~WS_EX_TRANSPARENT;
    SetWindowLongPtrW(handle, GWL_EXSTYLE, style);
    SetWindowPos(handle, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE
                     | SWP_FRAMECHANGED);
#else
    setWindowFlag(Qt::WindowTransparentForInput, enabled);
    showFullScreen();
#endif
    _bar->raise();
    if (!enabled) {
        activateWindow();
        setFocus();
    }
    if (changed)
        emit statusMessage(enabled ? tr("桌面交互已开启")
                                   : tr("桌面批注已开启"));
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
    _bar->setBrushes(_penColor, _penWidth, _markerColor, _markerWidth);
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
    saveScreenInk();
}

void EBDesktopOverlay::redo()
{
    if (!canRedo())
        return;
    ++_applied;
    update();
    refreshHistory();
    saveScreenInk();
}

void EBDesktopOverlay::clear()
{
    if (_drawing)
        _drawing = false;
    _strokes.clear();
    _applied = 0;
    update();
    refreshHistory();
    saveScreenInk();
}

void EBDesktopOverlay::captureToBoard()
{
    if (_capturing)
        return;
    _capturePath.clear();
    startCapture();
}

void EBDesktopOverlay::saveSnapshot()
{
    if (_capturing)
        return;
    const QString path = QFileDialog::getSaveFileName(this,
        tr("保存桌面批注图片"), QStringLiteral("desktop.png"),
        tr("PNG 图片 (*.png);;JPEG 图片 (*.jpg *.jpeg)"));
    if (path.isEmpty())
        return;
    _capturePath = path;
    startCapture();
}

void EBDesktopOverlay::startCapture()
{
    if (_drawing)
        finishStroke();
    _capturing = true;
    const QRect area = geometry();
    const qreal ratio = _activeScreen
        ? _activeScreen->devicePixelRatio() : devicePixelRatioF();
    hide();
    QTimer::singleShot(200, this, [this, area, ratio]() {
        finishCapture(area, ratio, 0);
    });
}

void EBDesktopOverlay::finishCapture(const QRect &area, qreal ratio, int attempt)
{
    QImage image = ebCaptureScreens(area, ratio);
    if (image.isNull() && attempt < 2) {
        QTimer::singleShot(200, this, [this, area, ratio, attempt]() {
            finishCapture(area, ratio, attempt + 1);
        });
        return;
    }
    _capturing = false;
    if (image.isNull()) {
        openOnDesktop();
        emit statusMessage(tr("无法读取桌面画面，请确认屏幕捕获权限"));
        return;
    }
    const QImage result = compositeImage(image);
    if (_capturePath.isEmpty()) {
        emit imageCaptured(result);
        return;
    }
    const QString path = _capturePath;
    _capturePath.clear();
    const QByteArray format = path.endsWith(QStringLiteral(".png"),
                                            Qt::CaseInsensitive) ? "PNG" : "JPEG";
    QSaveFile file(path);
    QImageWriter writer(&file, format);
    if (format == "JPEG")
        writer.setQuality(92);
    const bool saved = file.open(QIODevice::WriteOnly)
        && writer.write(result) && file.commit();
    openOnDesktop();
    emit statusMessage(saved ? tr("桌面批注已保存：%1").arg(path)
                             : tr("无法保存桌面批注图片：%1").arg(path));
}

QImage EBDesktopOverlay::compositeImage(QImage background) const
{
    if (background.isNull())
        return background;
    const qreal ratio = background.devicePixelRatioF();
    QImage marks(background.size(), QImage::Format_ARGB32_Premultiplied);
    marks.fill(Qt::transparent);
    QPainter marksPainter(&marks);
    marksPainter.setRenderHint(QPainter::Antialiasing);
    marksPainter.scale(ratio, ratio);
    for (int index = 0; index < _applied; ++index)
        paintStroke(&marksPainter, _strokes.at(index));
    marksPainter.end();
    background.setDevicePixelRatio(1.0);
    QPainter painter(&background);
    painter.drawImage(QPoint(0, 0), marks);
    painter.end();
    background.setDevicePixelRatio(ratio);
    return background;
}

void EBDesktopOverlay::followScreen()
{
    QScreen *screen = _targetScreen ? _targetScreen.data()
                                    : QGuiApplication::primaryScreen();
    if (screen) {
        const bool screenChanged = _activeScreen != screen;
        const QSize size = screen->geometry().size();
        if (_activeScreen != screen) {
            if (_drawing)
                finishStroke();
            saveScreenInk();
            _activeScreen = screen;
            restoreScreenInk(screen, size);
        } else if (_inkSize != size) {
            resizeInk(size);
        }
        if (windowHandle() && windowHandle()->screen() != screen)
            windowHandle()->setScreen(screen);
        setGeometry(screen->geometry());
        _bar->setCurrentScreen(screen);
        positionBar(screenChanged);
        if (screenChanged && _interactionMode && isVisible())
            setInteractionMode(true);
        update();
    }
}

void EBDesktopOverlay::saveScreenInk()
{
    if (!_activeScreen)
        return;
    const ScreenInk ink{_strokes, _applied, _inkSize};
    _screenInk.insert(_activeScreen, ink);
    _savedInk.insert(EBDesktopInkStore::screenId(_activeScreen), ink);
    _saveTimer->start();
}

void EBDesktopOverlay::restoreScreenInk(QScreen *screen, const QSize &size)
{
    const ScreenInk ink = _screenInk.contains(screen)
        ? _screenInk.value(screen)
        : _savedInk.value(EBDesktopInkStore::screenId(screen));
    _strokes = ink.strokes;
    _applied = qMin(ink.applied, _strokes.size());
    _inkSize = ink.size;
    resizeInk(size);
    refreshHistory();
}

void EBDesktopOverlay::resizeInk(const QSize &size)
{
    if (_inkSize.isValid() && size.isValid() && _inkSize != size) {
        const QTransform transform = QTransform::fromScale(
            qreal(size.width()) / _inkSize.width(),
            qreal(size.height()) / _inkSize.height());
        for (Stroke &stroke : _strokes)
            stroke.path = transform.map(stroke.path);
        _inkSize = size;
        saveScreenInk();
    }
    _inkSize = size;
    update();
}

void EBDesktopOverlay::selectScreen(QScreen *screen)
{
    if (!screen || screen == _targetScreen)
        return;
    _targetScreen = screen;
    followScreen();
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
    // Windows 会让全透明的分层窗口透过鼠标；保留极低不透明度以接收绘制输入。
    // 最后填充可让橡皮擦清除笔迹后，擦除区域仍能再次绘制。
    painter.setCompositionMode(QPainter::CompositionMode_DestinationOver);
    painter.fillRect(rect(), QColor(127, 127, 127, 1));
}

void EBDesktopOverlay::hideEvent(QHideEvent *event)
{
    _bar->hide();
    QWidget::hideEvent(event);
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
    positionBar(false);
}

void EBDesktopOverlay::positionBar(bool center)
{
    _bar->adjustSize();
    const QRect area = geometry();
    if (center || !_barPositioned) {
        _bar->move(area.left() + (area.width() - _bar->width()) / 2,
                   area.top() + 12);
        _barPositioned = true;
        return;
    }
    _bar->move(qBound(area.left(), _bar->x(),
                      qMax(area.left(), area.right() - _bar->width() + 1)),
               qBound(area.top(), _bar->y(),
                      qMax(area.top(), area.bottom() - _bar->height() + 1)));
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
    _strokeOrigin = event->pos();
    _drawing = true;
    event->accept();
}

void EBDesktopOverlay::mouseMoveEvent(QMouseEvent *event)
{
    if (!_drawing)
        return;
    if (_tool == Tool::Line || _tool == Tool::Rectangle
        || _tool == Tool::Ellipse) {
        updateShape(event->pos());
        update();
    } else if (QLineF(_current.path.currentPosition(), event->pos()).length() >= 1.0) {
        _current.path.lineTo(event->pos());
        update();
    }
    event->accept();
}

void EBDesktopOverlay::mouseReleaseEvent(QMouseEvent *event)
{
    if (!_drawing || event->button() != Qt::LeftButton)
        return;
    if (_tool == Tool::Line || _tool == Tool::Rectangle
        || _tool == Tool::Ellipse)
        updateShape(event->pos());
    else if (QLineF(_current.path.currentPosition(), event->pos()).length() >= 1.0)
        _current.path.lineTo(event->pos());
    finishStroke();
    event->accept();
}

void EBDesktopOverlay::updateShape(const QPointF &end)
{
    _current.path = QPainterPath();
    if (_tool == Tool::Line) {
        _current.path.moveTo(_strokeOrigin);
        _current.path.lineTo(end);
    } else if (_tool == Tool::Rectangle) {
        _current.path.addRect(QRectF(_strokeOrigin, end).normalized());
    } else if (_tool == Tool::Ellipse) {
        _current.path.addEllipse(QRectF(_strokeOrigin, end).normalized());
    }
}

void EBDesktopOverlay::finishStroke()
{
    if (!_drawing)
        return;
    if ((_tool == Tool::Rectangle || _tool == Tool::Ellipse)
        && (_current.path.boundingRect().width() < 2.0
            || _current.path.boundingRect().height() < 2.0)) {
        _drawing = false;
        update();
        return;
    }
    if (_current.path.elementCount() == 1)
        _current.path.lineTo(_current.path.currentPosition()
                             + QPointF(0.1, 0.1));
    _strokes.resize(_applied);
    _strokes.append(_current);
    ++_applied;
    _drawing = false;
    update();
    refreshHistory();
    saveScreenInk();
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
