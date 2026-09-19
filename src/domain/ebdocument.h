#ifndef EBDOCUMENT_H
#define EBDOCUMENT_H

#include <QVector>
#include <QDateTime>
#include <QString>

#include "ebpage.h"

class EBDocumentStorage;

// 一个文档至少包含一页，页面顺序与缩略图导航顺序一致。
class EBDocument
{
public:
    EBDocument();

    QString id() const;
    QString title() const;
    QDateTime createdAt() const;

    int pageCount() const;
    int currentPageIndex() const;
    int addPage();
    int duplicatePage(int index);
    bool removePage(int index);
    bool movePage(int from, int to);
    bool setCurrentPageIndex(int index);
    const EBPage *pageAt(int index) const;

    EBPage *currentPage();
    const EBPage *currentPage() const;

private:
    friend class EBDocumentStorage;

    QVector<EBPage> _pages;
    int _currentPageIndex;
    QString _id;
    QString _title;
    QDateTime _createdAt;
};

#endif
