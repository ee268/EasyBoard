#ifndef EBDOCUMENT_H
#define EBDOCUMENT_H

#include <QVector>
#include <QDateTime>
#include <QString>

#include "ebpage.h"

class EBDocumentStorage;
class EBDocumentPackage;

// 一个文档至少包含一页，页面顺序与缩略图导航顺序一致。
class EBDocument
{
public:
    EBDocument();

    QString id() const;
    QString title() const;
    bool setTitle(const QString &title);
    QDateTime createdAt() const;

    int pageCount() const;
    int currentPageIndex() const;
    int addPage(qreal width = 1200.0, qreal height = EBPage::Height);
    int duplicatePage(int index);
    bool removePage(int index);
    bool movePage(int from, int to);
    bool setCurrentPageIndex(int index);
    const EBPage *pageAt(int index) const;

    EBPage *currentPage();
    const EBPage *currentPage() const;

private:
    friend class EBDocumentStorage;
    friend class EBDocumentPackage;

    QVector<EBPage> _pages;
    int _currentPageIndex;
    QString _id;
    QString _title;
    QDateTime _createdAt;
};

#endif
