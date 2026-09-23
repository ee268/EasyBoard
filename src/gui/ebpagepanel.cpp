#include "ebpagepanel.h"

#include <QHBoxLayout>
#include <QIcon>
#include <QImage>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include "../board/ebboardscene.h"
#include "../domain/ebdocument.h"

namespace {
constexpr int kThumbnailWidth = 144;
constexpr int kThumbnailHeight = 108;

QIcon pageIcon(const EBPage &page)
{
    // 使用与画板相同的场景绘制缩略图，笔迹、底色和底纹保持一致。
    EBBoardScene scene;
    scene.showPage(page);
    QImage image(kThumbnailWidth, kThumbnailHeight,
                 QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    scene.render(&painter, QRectF(0, 0, kThumbnailWidth, kThumbnailHeight),
                 scene.pageRect());
    return QIcon(QPixmap::fromImage(image));
}
}

EBPagePanel::EBPagePanel(EBDocument *document, QWidget *parent)
    : QWidget(parent)
    , _document(document)
    , _pageList(new QListWidget(this))
    , _pageNumber(new QLabel(this))
    , _removeButton(new QPushButton(tr("删除"), this))
    , _moveUpButton(new QPushButton(tr("上移"), this))
    , _moveDownButton(new QPushButton(tr("下移"), this))
    , _previousButton(new QPushButton(tr("上一页"), this))
    , _nextButton(new QPushButton(tr("下一页"), this))
{
    setObjectName(QStringLiteral("pageNavigator"));
    setFixedWidth(210);
    setStyleSheet(QStringLiteral(
        "EBPagePanel { background: #F8FAFB; border-right: 1px solid #DCE6EA; }"
        "QListWidget { background: transparent; border: none; outline: none; }"
        "QListWidget::item { border: 1px solid transparent; border-radius: 8px; margin: 3px; padding: 4px; }"
        "QListWidget::item:selected { background: #DDF3EF; border-color: #A5DDD4; color: #193A3A; }"));

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 10, 8, 10);
    layout->setSpacing(8);
    QHBoxLayout *header = new QHBoxLayout;
    QLabel *title = new QLabel(tr("页面"), this);
    QPushButton *addButton = new QPushButton(tr("新增"), this);
    QPushButton *duplicateButton = new QPushButton(tr("复制"), this);
    addButton->setObjectName(QStringLiteral("addPageButton"));
    addButton->setToolTip(tr("新增页面"));
    duplicateButton->setObjectName(QStringLiteral("duplicatePageButton"));
    duplicateButton->setToolTip(tr("复制当前页面"));
    _removeButton->setObjectName(QStringLiteral("removePageButton"));
    _removeButton->setToolTip(tr("删除当前页面"));
    header->addWidget(title);
    header->addStretch();
    header->addWidget(addButton);
    header->addWidget(duplicateButton);
    header->addWidget(_removeButton);
    layout->addLayout(header);

    _pageList->setObjectName(QStringLiteral("pageThumbnailList"));
    _pageList->setViewMode(QListView::IconMode);
    _pageList->setFlow(QListView::TopToBottom);
    _pageList->setWrapping(false);
    _pageList->setResizeMode(QListView::Adjust);
    _pageList->setMovement(QListView::Static);
    _pageList->setIconSize(QSize(kThumbnailWidth, kThumbnailHeight));
    _pageList->setGridSize(QSize(164, 146));
    layout->addWidget(_pageList, 1);

    QHBoxLayout *order = new QHBoxLayout;
    _moveUpButton->setObjectName(QStringLiteral("movePageUpButton"));
    _moveDownButton->setObjectName(QStringLiteral("movePageDownButton"));
    _moveUpButton->setToolTip(tr("将当前页面向前移动"));
    _moveDownButton->setToolTip(tr("将当前页面向后移动"));
    order->addWidget(_moveUpButton);
    order->addWidget(_moveDownButton);
    layout->addLayout(order);

    QHBoxLayout *footer = new QHBoxLayout;
    _previousButton->setObjectName(QStringLiteral("previousPageButton"));
    _nextButton->setObjectName(QStringLiteral("nextPageButton"));
    _pageNumber->setObjectName(QStringLiteral("pageNumberLabel"));
    _pageNumber->setAlignment(Qt::AlignCenter);
    footer->addWidget(_previousButton);
    footer->addWidget(_pageNumber, 1);
    footer->addWidget(_nextButton);
    layout->addLayout(footer);

    connect(addButton, &QPushButton::clicked, this, &EBPagePanel::addPageRequested);
    connect(duplicateButton, &QPushButton::clicked,
            this, &EBPagePanel::duplicatePageRequested);
    connect(_removeButton, &QPushButton::clicked,
            this, &EBPagePanel::removePageRequested);
    connect(_moveUpButton, &QPushButton::clicked, this, [this]() {
        emit movePageRequested(-1);
    });
    connect(_moveDownButton, &QPushButton::clicked, this, [this]() {
        emit movePageRequested(1);
    });
    connect(_pageList, &QListWidget::currentRowChanged, this, [this](int index) {
        if (index >= 0 && index != _document->currentPageIndex())
            emit pageSelected(index);
    });
    connect(_previousButton, &QPushButton::clicked, this, [this]() {
        emit pageSelected(_document->currentPageIndex() - 1);
    });
    connect(_nextButton, &QPushButton::clicked, this, [this]() {
        emit pageSelected(_document->currentPageIndex() + 1);
    });
    refreshPages();
}

void EBPagePanel::refreshPages()
{
    QSignalBlocker blocker(_pageList);
    _pageList->clear();
    for (int index = 0; index < _document->pageCount(); ++index) {
        QListWidgetItem *item = new QListWidgetItem(pageIcon(*_document->pageAt(index)),
                                                     tr("第 %1 页").arg(index + 1));
        _pageList->addItem(item);
    }
    _pageList->setCurrentRow(_document->currentPageIndex());
    updateControls();
}

void EBPagePanel::refreshPage(int index)
{
    const EBPage *page = _document->pageAt(index);
    if (page && index < _pageList->count())
        _pageList->item(index)->setIcon(pageIcon(*page));
}

void EBPagePanel::setCurrentPageIndex(int index)
{
    QSignalBlocker blocker(_pageList);
    _pageList->setCurrentRow(index);
    updateControls();
}

void EBPagePanel::updateControls()
{
    const int index = _document->currentPageIndex();
    _pageNumber->setText(tr("%1 / %2").arg(index + 1).arg(_document->pageCount()));
    _previousButton->setEnabled(index > 0);
    _nextButton->setEnabled(index + 1 < _document->pageCount());
    _removeButton->setEnabled(_document->pageCount() > 1);
    _moveUpButton->setEnabled(index > 0);
    _moveDownButton->setEnabled(index + 1 < _document->pageCount());
}
