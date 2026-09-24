#ifndef EBDESKTOPOVERLAY_H
#define EBDESKTOPOVERLAY_H

#include <QColor>
#include <QHash>
#include <QImage>
#include <QPainterPath>
#include <QPointer>
#include <QVector>
#include <QWidget>

class EBDesktopBar;
class QScreen;

// 桌面批注只保存在当前会话；透明窗口上按绘制顺序重放笔迹和局部擦除。
class EBDesktopOverlay : public QWidget
{
    Q_OBJECT
public:
    enum class Tool { Pen, Marker, Eraser, Line, Rectangle, Ellipse };

    explicit EBDesktopOverlay(QWidget *parent = nullptr);

    void openOnDesktop();
    void setBrushes(const QColor &penColor, qreal penWidth,
                    const QColor &markerColor, qreal markerWidth);
    void setTool(Tool tool);
    Tool tool() const;
    int strokeCount() const;
    bool canUndo() const;
    bool canRedo() const;
    void undo();
    void redo();
    void clear();
    void captureToBoard();
    void saveSnapshot();
    void setInteractionMode(bool enabled);
    QImage compositeImage(QImage background) const;
    void selectScreen(QScreen *screen);

signals:
    void exitRequested();
    void imageCaptured(const QImage &image);
    void statusMessage(const QString &message);
    void historyAvailabilityChanged(bool undoAvailable, bool redoAvailable);

protected:
    void paintEvent(QPaintEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    struct Stroke {
        Tool tool;
        QPainterPath path;
        QColor color;
        qreal width;
    };
    struct ScreenInk {
        QVector<Stroke> strokes;
        int applied = 0;
        QSize size;
    };

    void paintStroke(QPainter *painter, const Stroke &stroke) const;
    void updateShape(const QPointF &end);
    void saveScreenInk();
    void restoreScreenInk(QScreen *screen, const QSize &size);
    void resizeInk(const QSize &size);
    void finishStroke();
    void refreshHistory();
    void followScreen();
    void positionBar(bool center);
    void finishCapture(const QRect &area, qreal ratio, int attempt);
    void startCapture();

    EBDesktopBar *_bar;
    QVector<Stroke> _strokes;
    QHash<QScreen *, ScreenInk> _screenInk;
    QPointer<QScreen> _activeScreen;
    QSize _inkSize;
    int _applied = 0;
    Stroke _current;
    QPointF _strokeOrigin;
    Tool _tool = Tool::Pen;
    QColor _penColor = QColor(36, 58, 72);
    QColor _markerColor = QColor(255, 214, 59, 115);
    qreal _penWidth = 3.0;
    qreal _markerWidth = 18.0;
    bool _drawing = false;
    bool _barPositioned = false;
    bool _capturing = false;
    bool _interactionMode = false;
    QString _capturePath;
    QPointer<QScreen> _targetScreen;
};

#endif
