#ifndef EBPAGEPANEL_H
#define EBPAGEPANEL_H

#include <QWidget>

class EBDocument;
class QListWidget;
class QPushButton;
class QLabel;

// 左侧缩略图列表展示文档页面，并转发翻页与页面管理意图。
class EBPagePanel : public QWidget
{
    Q_OBJECT

public:
    explicit EBPagePanel(EBDocument *document, QWidget *parent = nullptr);

    void refreshPages();
    void refreshPage(int index);
    void setCurrentPageIndex(int index);

signals:
    void pageSelected(int index);
    void addPageRequested();
    void duplicatePageRequested();
    void removePageRequested();
    void movePageRequested(int offset);

private:
    void updateControls();

    EBDocument *_document;
    QListWidget *_pageList;
    QLabel *_pageNumber;
    QPushButton *_removeButton;
    QPushButton *_moveUpButton;
    QPushButton *_moveDownButton;
    QPushButton *_previousButton;
    QPushButton *_nextButton;
};

#endif
