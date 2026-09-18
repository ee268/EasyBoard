#include "ebdocument.h"

EBDocument::EBDocument()
    : _pages(1)
    , _currentPageIndex(0)
{
}

int EBDocument::pageCount() const
{
    return _pages.size();
}

int EBDocument::currentPageIndex() const
{
    return _currentPageIndex;
}

int EBDocument::addPage()
{
    _pages.append(EBPage());
    return _pages.size() - 1;
}

bool EBDocument::setCurrentPageIndex(int index)
{
    if (index < 0 || index >= _pages.size())
        return false;
    _currentPageIndex = index;
    return true;
}

const EBPage *EBDocument::pageAt(int index) const
{
    if (index < 0 || index >= _pages.size())
        return nullptr;
    return &_pages[index];
}

EBPage *EBDocument::currentPage()
{
    return &_pages[_currentPageIndex];
}

const EBPage *EBDocument::currentPage() const
{
    return &_pages[_currentPageIndex];
}
