#ifndef EBDRAWBAR_H
#define EBDRAWBAR_H

#include <QToolBar>

class QAction;
class QMainWindow;
class QToolButton;
class EBBoardView;

// 底部绘图工具栏管理工具切换、画笔参数和文字格式入口。
class EBDrawBar : public QToolBar
{
    Q_OBJECT
public:
    EBDrawBar(QMainWindow *window, EBBoardView *boardView);

    void setBoardActive(bool active);
    void retranslate();
    void updateTextFormat();

private:
    void createDrawingActions();
    void createBrushMenu();
    void createTextFormatMenu();
    void createTeachingMenu();

    QMainWindow *_window;
    EBBoardView *_boardView;
    QToolButton *_brushButton = nullptr;
    QToolButton *_shapeButton = nullptr;
    QToolButton *_textFormatButton = nullptr;
    QToolButton *_teachingButton = nullptr;
    QAction *_toolActions[11] = {};
    bool _boardModeActive = false;
};

#endif
