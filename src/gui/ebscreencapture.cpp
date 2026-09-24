#include "ebscreencapture.h"

#include <QGuiApplication>
#include <QPainter>
#include <QPixmap>
#include <QRegion>
#include <QScreen>
#include <QtMath>

#ifdef Q_OS_WIN
#include <windows.h>
#pragma comment(lib, "gdi32.lib")
#endif

namespace {
bool entirelyBlack(const QImage &image)
{
    if (image.isNull())
        return true;
    for (int y = 0; y < image.height(); y += qMax(1, image.height() / 16)) {
        for (int x = 0; x < image.width(); x += qMax(1, image.width() / 16)) {
            if (image.pixel(x, y) & 0x00ffffff)
                return false;
        }
    }
    return true;
}

#ifdef Q_OS_WIN
struct MonitorOrigin {
    QString name;
    QPoint position;
    bool found = false;
};

BOOL CALLBACK findMonitorOrigin(HMONITOR monitor, HDC, LPRECT, LPARAM context)
{
    MonitorOrigin *result = reinterpret_cast<MonitorOrigin *>(context);
    MONITORINFOEXW info = {};
    info.cbSize = sizeof(info);
    if (GetMonitorInfoW(monitor, &info)
        && QString::fromWCharArray(info.szDevice).compare(
            result->name, Qt::CaseInsensitive) == 0) {
        result->position = QPoint(info.rcMonitor.left, info.rcMonitor.top);
        result->found = true;
        return FALSE;
    }
    return TRUE;
}

QImage captureWithGdi(const QRect &area, QScreen *screen, qreal ratio)
{
    const int width = qCeil(area.width() * ratio);
    const int height = qCeil(area.height() * ratio);
    MonitorOrigin origin{screen->name(), screen->geometry().topLeft()};
    EnumDisplayMonitors(nullptr, nullptr, findMonitorOrigin,
                        reinterpret_cast<LPARAM>(&origin));
    const QPoint local = area.topLeft() - screen->geometry().topLeft();
    const int sourceX = origin.position.x() + qRound(local.x() * ratio);
    const int sourceY = origin.position.y() + qRound(local.y() * ratio);
    HDC desktop = GetDC(nullptr);
    if (!desktop)
        return QImage();
    HDC memory = CreateCompatibleDC(desktop);
    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void *pixels = nullptr;
    HBITMAP bitmap = CreateDIBSection(desktop, &info, DIB_RGB_COLORS,
                                      &pixels, nullptr, 0);
    QImage image;
    if (memory && bitmap && pixels) {
        HGDIOBJ previous = SelectObject(memory, bitmap);
        if (BitBlt(memory, 0, 0, width, height, desktop,
                   sourceX, sourceY,
                   SRCCOPY | CAPTUREBLT)) {
            image = QImage(static_cast<uchar *>(pixels), width, height,
                           width * 4, QImage::Format_RGB32).copy();
        }
        SelectObject(memory, previous);
    }
    if (bitmap)
        DeleteObject(bitmap);
    if (memory)
        DeleteDC(memory);
    ReleaseDC(nullptr, desktop);
    return image;
}
#endif
}

QImage ebCaptureScreens(const QRect &area, qreal devicePixelRatio)
{
    if (area.isEmpty())
        return QImage();
    const qreal ratio = qMax(1.0, devicePixelRatio);
    QImage result(qCeil(area.width() * ratio), qCeil(area.height() * ratio),
                  QImage::Format_ARGB32_Premultiplied);
    result.fill(Qt::transparent);
    QRegion uncovered(area);
    QPainter painter(&result);
    painter.scale(ratio, ratio);
    for (QScreen *screen : QGuiApplication::screens()) {
        const QRect part = area.intersected(screen->geometry());
        if (part.isEmpty())
            continue;
        const QPixmap shot = screen->grabWindow(0);
        const QImage screenImage = shot.toImage();
        const qreal sourceRatio = shot.isNull() ? screen->devicePixelRatio()
                                                 : shot.devicePixelRatioF();
        const QRectF target(part.topLeft() - area.topLeft(), part.size());
#ifdef Q_OS_WIN
        if (entirelyBlack(screenImage)) {
            const QImage fallback = captureWithGdi(part, screen, sourceRatio);
            if (fallback.isNull())
                continue;
            painter.drawImage(target, fallback);
        } else
#endif
        {
            if (screenImage.isNull())
                continue;
            const QRectF source((part.x() - screen->geometry().x()) * sourceRatio,
                                (part.y() - screen->geometry().y()) * sourceRatio,
                                part.width() * sourceRatio,
                                part.height() * sourceRatio);
            painter.drawImage(target, screenImage, source);
        }
        uncovered -= part;
    }
    painter.end();
    if (!uncovered.isEmpty())
        return QImage();
    result.setDevicePixelRatio(ratio);
    return result;
}
