#ifndef EBPAGENAVIGATOR_H
#define EBPAGENAVIGATOR_H

#include <QWidget>

class EBDocument;
class QListWidget;
class QPushButton;
class QLabel;

// 左侧缩略图列表只展示文档页面，并把翻页与新增页意图交给主窗口。
class EBPageNavigator : public QWidget
{
    Q_OBJECT

public:
    explicit EBPageNavigator(EBDocument *document, QWidget *parent = nullptr);

    void refreshPages();
    void refreshPage(int index);
    void setCurrentPageIndex(int index);

signals:
    void pageSelected(int index);
    void addPageRequested();

private:
    void updateControls();

    EBDocument *_document;
    QListWidget *_pageList;
    QLabel *_pageNumber;
    QPushButton *_previousButton;
    QPushButton *_nextButton;
};

#endif
