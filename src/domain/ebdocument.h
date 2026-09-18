#ifndef EBDOCUMENT_H
#define EBDOCUMENT_H

#include <QVector>

#include "ebpage.h"

// 一个文档至少包含一页，页面顺序与缩略图导航顺序一致。
class EBDocument
{
public:
    EBDocument();

    int pageCount() const;
    int currentPageIndex() const;
    int addPage();
    bool setCurrentPageIndex(int index);
    const EBPage *pageAt(int index) const;

    EBPage *currentPage();
    const EBPage *currentPage() const;

private:
    QVector<EBPage> _pages;
    int _currentPageIndex;
};

#endif
