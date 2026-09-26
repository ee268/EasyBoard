#include "ebdocument.h"

#include <QUuid>

EBDocument::EBDocument()
    : _pages(1)
    , _currentPageIndex(0)
    , _id(QUuid::createUuid().toString(QUuid::WithoutBraces))
    , _title(QStringLiteral("未命名白板"))
    , _createdAt(QDateTime::currentDateTimeUtc())
{
}

QString EBDocument::id() const
{
    return _id;
}

QString EBDocument::title() const
{
    return _title;
}

bool EBDocument::setTitle(const QString &title)
{
    const QString normalized = title.trimmed();
    if (normalized.isEmpty() || normalized.size() > 120)
        return false;
    for (const QChar character : normalized) {
        if (!character.isPrint())
            return false;
    }
    _title = normalized;
    return true;
}

QDateTime EBDocument::createdAt() const
{
    return _createdAt;
}

int EBDocument::pageCount() const
{
    return _pages.size();
}

int EBDocument::currentPageIndex() const
{
    return _currentPageIndex;
}

int EBDocument::addPage(qreal width, qreal height)
{
    EBPage page;
    if (!page.setSizeForDimensions(width, height))
        return -1;
    _pages.append(page);
    return _pages.size() - 1;
}

int EBDocument::duplicatePage(int index)
{
    if (index < 0 || index >= _pages.size())
        return -1;
    const int copyIndex = index + 1;
    const EBPage copy = _pages.at(index);
    _pages.insert(copyIndex, copy);
    return copyIndex;
}

bool EBDocument::removePage(int index)
{
    // 文档必须始终保留一页，最后一页不能删除。
    if (_pages.size() <= 1 || index < 0 || index >= _pages.size())
        return false;
    _pages.removeAt(index);
    if (_currentPageIndex > index)
        --_currentPageIndex;
    else if (_currentPageIndex >= _pages.size())
        _currentPageIndex = _pages.size() - 1;
    return true;
}

bool EBDocument::movePage(int from, int to)
{
    if (from < 0 || from >= _pages.size()
        || to < 0 || to >= _pages.size() || from == to)
        return false;

    _pages.move(from, to);
    // 当前页索引始终跟随同一个页面对象，而不是停留在原来的序号。
    if (_currentPageIndex == from) {
        _currentPageIndex = to;
    } else if (from < _currentPageIndex && to >= _currentPageIndex) {
        --_currentPageIndex;
    } else if (from > _currentPageIndex && to <= _currentPageIndex) {
        ++_currentPageIndex;
    }
    return true;
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
