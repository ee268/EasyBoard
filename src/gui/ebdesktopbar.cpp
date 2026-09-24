#include "ebdesktopbar.h"

#include <QAction>
#include <QActionGroup>
#include <QColorDialog>
#include <QInputDialog>
#include <QGuiApplication>
#include <QMenu>
#include <QMouseEvent>
#include <QScreen>
#include <QToolButton>

#include "ebbarstyle.h"
#include "ebicons.h"

EBDesktopBar::EBDesktopBar(QWidget *parent)
    : QToolBar(tr("桌面批注"), parent)
    , _undoAction(nullptr)
    , _redoAction(nullptr)
{
    setObjectName(QStringLiteral("desktopAnnotationBar"));
    ebStyleBar(this, QStringLiteral("border: 1px solid #DCE6EA; border-radius: 10px;"), true);
    QToolButton *drag = new QToolButton(this);
    drag->setObjectName(QStringLiteral("desktopBarDragHandle"));
    drag->setText(tr("移动"));
    drag->setToolTip(tr("拖动工具栏"));
    drag->setCursor(Qt::SizeAllCursor);
    drag->installEventFilter(this);
    _dragHandle = drag;
    addWidget(drag);
    QActionGroup *tools = new QActionGroup(this);
    tools->setExclusive(true);
    const char *icons[] = {"pen", "marker", "eraser",
                           "line", "rectangle", "ellipse"};
    const char *names[] = {"desktopPenAction", "desktopMarkerAction",
                           "desktopEraserAction", "desktopLineAction",
                           "desktopRectangleAction", "desktopEllipseAction"};
    const QString labels[] = {tr("画笔"), tr("荧光笔"), tr("橡皮"),
                              tr("直线"), tr("矩形"), tr("椭圆")};
    for (int index = 0; index < 6; ++index) {
        QAction *action = addAction(ebToolbarIcon(icons[index]), labels[index]);
        action->setObjectName(QString::fromLatin1(names[index]));
        action->setCheckable(true);
        tools->addAction(action);
        connect(action, &QAction::triggered, this, [this, index]() {
            emit toolSelected(static_cast<EBDesktopOverlay::Tool>(index));
        });
        if (index == 0)
            action->setChecked(true);
    }
    QToolButton *brush = new QToolButton(this);
    brush->setObjectName(QStringLiteral("desktopBrushSettings"));
    brush->setIcon(ebToolbarIcon("brush_settings"));
    brush->setText(tr("画笔设置"));
    brush->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    brush->setPopupMode(QToolButton::InstantPopup);
    QMenu *menu = new QMenu(brush);
    QAction *penColor = menu->addAction(ebToolbarIcon("pen_color"), tr("画笔颜色"));
    QAction *penWidth = menu->addAction(ebToolbarIcon("pen_width"), tr("画笔粗细"));
    menu->addSeparator();
    QAction *markerColor = menu->addAction(ebToolbarIcon("marker_color"), tr("荧光笔颜色"));
    QAction *markerWidth = menu->addAction(ebToolbarIcon("marker_width"), tr("荧光笔粗细"));
    connect(penColor, &QAction::triggered, this, [this]() {
        const QColor color = QColorDialog::getColor(_penColor, this, tr("画笔颜色"));
        if (color.isValid()) {
            _penColor = color;
            emit penColorChanged(color);
        }
    });
    connect(markerColor, &QAction::triggered, this, [this]() {
        const QColor color = QColorDialog::getColor(_markerColor, this,
            tr("荧光笔颜色"), QColorDialog::ShowAlphaChannel);
        if (color.isValid()) {
            _markerColor = color;
            emit markerColorChanged(color);
        }
    });
    connect(penWidth, &QAction::triggered, this, [this]() {
        bool accepted = false;
        const qreal width = QInputDialog::getDouble(this, tr("画笔粗细"),
            tr("像素"), _penWidth, 1.0, 24.0, 1, &accepted);
        if (accepted) {
            _penWidth = width;
            emit penWidthChanged(width);
        }
    });
    connect(markerWidth, &QAction::triggered, this, [this]() {
        bool accepted = false;
        const qreal width = QInputDialog::getDouble(this, tr("荧光笔粗细"),
            tr("像素"), _markerWidth, 4.0, 48.0, 1, &accepted);
        if (accepted) {
            _markerWidth = width;
            emit markerWidthChanged(width);
        }
    });
    brush->setMenu(menu);
    addWidget(brush);
    addSeparator();
    _undoAction = addAction(ebToolbarIcon("undo"), tr("撤销"));
    _undoAction->setObjectName(QStringLiteral("desktopUndoAction"));
    _undoAction->setShortcut(QKeySequence::Undo);
    _redoAction = addAction(ebToolbarIcon("redo"), tr("重做"));
    _redoAction->setObjectName(QStringLiteral("desktopRedoAction"));
    _redoAction->setShortcut(QKeySequence::Redo);
    connect(_undoAction, &QAction::triggered,
            this, &EBDesktopBar::undoRequested);
    connect(_redoAction, &QAction::triggered,
            this, &EBDesktopBar::redoRequested);
    addSeparator();
    QAction *clear = addAction(ebToolbarIcon("object_delete"), tr("清空批注"));
    clear->setObjectName(QStringLiteral("desktopClearAction"));
    connect(clear, &QAction::triggered, this, &EBDesktopBar::clearRequested);
    QAction *capture = addAction(ebToolbarIcon("image_object"), tr("插入白板"));
    capture->setObjectName(QStringLiteral("desktopCaptureAction"));
    connect(capture, &QAction::triggered, this, &EBDesktopBar::captureRequested);
    QToolButton *screenButton = new QToolButton(this);
    screenButton->setObjectName(QStringLiteral("desktopScreenButton"));
    screenButton->setIcon(ebToolbarIcon("desktop"));
    screenButton->setText(tr("屏幕"));
    screenButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    screenButton->setPopupMode(QToolButton::InstantPopup);
    QMenu *screenMenu = new QMenu(screenButton);
    connect(screenMenu, &QMenu::aboutToShow, this, [this, screenMenu]() {
        screenMenu->clear();
        int number = 1;
        for (QScreen *screen : QGuiApplication::screens()) {
            const QSize size = screen->geometry().size();
            QAction *action = screenMenu->addAction(
                tr("屏幕 %1：%2 × %3").arg(number++)
                    .arg(size.width()).arg(size.height()));
            action->setCheckable(true);
            action->setChecked(screen == _currentScreen);
            QPointer<QScreen> selected = screen;
            connect(action, &QAction::triggered, this, [this, selected]() {
                if (selected)
                    emit screenSelected(selected);
            });
        }
    });
    screenButton->setMenu(screenMenu);
    addWidget(screenButton);
    addSeparator();
    QAction *exit = addAction(ebToolbarIcon("file_exit"), tr("返回白板"));
    exit->setObjectName(QStringLiteral("desktopExitAction"));
    connect(exit, &QAction::triggered,
            this, &EBDesktopBar::exitRequested);
    setHistory(false, false);
}

void EBDesktopBar::setHistory(bool undoAvailable, bool redoAvailable)
{
    _undoAction->setEnabled(undoAvailable);
    _redoAction->setEnabled(redoAvailable);
}

void EBDesktopBar::setBrushes(const QColor &penColor, qreal penWidth,
                             const QColor &markerColor, qreal markerWidth)
{
    _penColor = penColor;
    _penWidth = penWidth;
    _markerColor = markerColor;
    _markerWidth = markerWidth;
}

void EBDesktopBar::setCurrentScreen(QScreen *screen)
{
    _currentScreen = screen;
}

bool EBDesktopBar::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == _dragHandle) {
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *mouse = static_cast<QMouseEvent *>(event);
            if (mouse->button() == Qt::LeftButton) {
                _dragOrigin = mouse->globalPos() - pos();
                return true;
            }
        } else if (event->type() == QEvent::MouseMove) {
            QMouseEvent *mouse = static_cast<QMouseEvent *>(event);
            if (mouse->buttons() & Qt::LeftButton) {
                const QPoint target = mouse->globalPos() - _dragOrigin;
                QWidget *surface = parentWidget();
                if (surface)
                    move(qBound(0, target.x(), qMax(0, surface->width() - width())),
                         qBound(0, target.y(), qMax(0, surface->height() - height())));
                return true;
            }
        }
    }
    return QToolBar::eventFilter(watched, event);
}
