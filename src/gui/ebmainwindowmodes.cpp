#include "ebmainwindow.h"

#include <QBuffer>
#include <QImage>
#include <QStackedWidget>
#include <QStatusBar>

#include "../board/ebboardview.h"
#include "ebcommands.h"
#include "ebdesktopoverlay.h"
#include "ebdisplayview.h"
#include "ebwebworkspace.h"

EBMainWindow::~EBMainWindow()
{
    delete _webWorkspace;
    delete _desktopOverlay;
}

void EBMainWindow::ensureWebWorkspace()
{
    if (_webWorkspace)
        return;
    _webWorkspace = new EBWebWorkspace(_modeStack);
    _modeStack->removeWidget(_webPlaceholder);
    delete _webPlaceholder;
    _webPlaceholder = nullptr;
    _modeStack->insertWidget(2, _webWorkspace);
    connect(_webWorkspace, &EBWebWorkspace::imageCaptured,
            this, &EBMainWindow::insertCapturedWebImage);
    connect(_webWorkspace, &EBWebWorkspace::statusMessage,
            this, [this](const QString &message) {
        statusBar()->showMessage(message, 5000);
    });
}

void EBMainWindow::ensureDesktopOverlay()
{
    if (_desktopOverlay)
        return;
    _desktopOverlay = new EBDesktopOverlay();
    connect(_desktopOverlay, &EBDesktopOverlay::exitRequested,
            this, [this]() {
        emit modeRequested(EBApplicationController::MainMode::Board);
    });
    connect(_desktopOverlay, &EBDesktopOverlay::imageCaptured,
            this, &EBMainWindow::insertCapturedDesktopImage);
    connect(_desktopOverlay, &EBDesktopOverlay::statusMessage,
            this, [this](const QString &message) {
        statusBar()->showMessage(message, 5000);
    });
}

void EBMainWindow::insertCapturedWebImage(const QImage &image)
{
    insertCapturedImage(image, tr("网页"));
}

void EBMainWindow::insertCapturedDesktopImage(const QImage &image)
{
    if (!insertCapturedImage(image, tr("桌面")) && _desktopOverlay)
        _desktopOverlay->openOnDesktop();
}

bool EBMainWindow::insertCapturedImage(const QImage &image,
                                      const QString &source)
{
    if (image.isNull()) {
        statusBar()->showMessage(tr("%1截图失败").arg(source), 5000);
        return false;
    }
    EBImageItem::State state;
    state.format = EBImageItem::Format::Png;
    state.size = QSizeF(image.width() / image.devicePixelRatioF(),
                        image.height() / image.devicePixelRatioF());
    QBuffer buffer(&state.data);
    if (!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, "PNG")
        || !_boardView->insertImageObject(state)) {
        statusBar()->showMessage(tr("%1截图无法插入白板").arg(source), 5000);
        return false;
    }
    emit modeRequested(EBApplicationController::MainMode::Board);
    statusBar()->showMessage(tr("%1截图已插入当前白板页").arg(source), 5000);
    return true;
}

void EBMainWindow::showMode(EBApplicationController::MainMode mode)
{
    const int index = static_cast<int>(mode);
    if (index < 0 || index >= 4)
        return;
    if (mode == EBApplicationController::MainMode::Document)
        refreshDocumentLibrary();
    if (mode == EBApplicationController::MainMode::Web)
        ensureWebWorkspace();
    if (mode != EBApplicationController::MainMode::Board && _displayView)
        _displayView->close();
    if (mode != EBApplicationController::MainMode::Desktop
        && _desktopOverlay && _modeStack->currentIndex() == 3) {
        show();
        _desktopOverlay->hide();
        raise();
        activateWindow();
    }
    _modeStack->setCurrentIndex(index);
    _commands->setMode(mode);
    if (mode == EBApplicationController::MainMode::Desktop) {
        ensureDesktopOverlay();
        _desktopOverlay->setBrushes(_boardView->penColor(), _boardView->penWidth(),
                                    _boardView->markerColor(),
                                    _boardView->markerWidth());
        _desktopOverlay->openOnDesktop();
        hide();
    }
}

void EBMainWindow::setDisplayVisible(bool visible)
{
    if (!visible) {
        if (_displayView)
            _displayView->close();
        return;
    }
    if (!_displayView) {
        _displayView = new EBDisplayView(_boardView, this);
        connect(_displayView, &EBDisplayView::displayClosed,
                this, [this]() { _commands->setDisplayVisible(false); });
    }
    _displayView->openOnPreferredScreen();
}
