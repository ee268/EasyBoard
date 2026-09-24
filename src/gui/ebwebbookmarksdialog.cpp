#include "ebwebbookmarksdialog.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include "ebwebbookmarks.h"

EBWebBookmarksDialog::EBWebBookmarksDialog(EBWebBookmarks *bookmarks,
                                           QWidget *parent)
    : QDialog(parent)
    , _bookmarks(bookmarks)
    , _search(new QLineEdit(this))
    , _table(new QTableWidget(this))
{
    setWindowTitle(tr("管理书签"));
    resize(780, 480);
    QVBoxLayout *layout = new QVBoxLayout(this);
    _search->setPlaceholderText(tr("搜索名称、分组或网址"));
    layout->addWidget(_search);
    _table->setColumnCount(3);
    _table->setHorizontalHeaderLabels({tr("名称"), tr("分组"), tr("网址")});
    _table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    _table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    _table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    _table->setSelectionBehavior(QAbstractItemView::SelectRows);
    _table->setSelectionMode(QAbstractItemView::SingleSelection);
    _table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(_table);
    QHBoxLayout *actions = new QHBoxLayout;
    QPushButton *edit = new QPushButton(tr("编辑"), this);
    QPushButton *remove = new QPushButton(tr("删除"), this);
    QPushButton *open = new QPushButton(tr("打开"), this);
    QPushButton *close = new QPushButton(tr("关闭"), this);
    actions->addWidget(edit);
    actions->addWidget(remove);
    actions->addStretch();
    actions->addWidget(open);
    actions->addWidget(close);
    layout->addLayout(actions);
    connect(_search, &QLineEdit::textChanged,
            this, &EBWebBookmarksDialog::refresh);
    connect(_bookmarks, &EBWebBookmarks::changed,
            this, &EBWebBookmarksDialog::refresh);
    connect(_table, &QTableWidget::cellDoubleClicked,
            this, &EBWebBookmarksDialog::openSelected);
    connect(edit, &QPushButton::clicked,
            this, &EBWebBookmarksDialog::editSelected);
    connect(remove, &QPushButton::clicked,
            this, &EBWebBookmarksDialog::removeSelected);
    connect(open, &QPushButton::clicked,
            this, &EBWebBookmarksDialog::openSelected);
    connect(close, &QPushButton::clicked, this, &QDialog::reject);
    refresh();
}

void EBWebBookmarksDialog::refresh()
{
    _table->setRowCount(0);
    const QString query = _search->text().trimmed();
    const auto &entries = _bookmarks->entries();
    for (int index = 0; index < entries.size(); ++index) {
        const EBWebBookmarks::Entry &entry = entries.at(index);
        const QString address = entry.url.toString();
        if (!query.isEmpty() && !entry.title.contains(query, Qt::CaseInsensitive)
            && !entry.folder.contains(query, Qt::CaseInsensitive)
            && !address.contains(query, Qt::CaseInsensitive))
            continue;
        const int row = _table->rowCount();
        _table->insertRow(row);
        QTableWidgetItem *title = new QTableWidgetItem(
            entry.title.isEmpty() ? address : entry.title);
        title->setData(Qt::UserRole, index);
        _table->setItem(row, 0, title);
        _table->setItem(row, 1, new QTableWidgetItem(entry.folder));
        _table->setItem(row, 2, new QTableWidgetItem(address));
    }
}

int EBWebBookmarksDialog::selectedIndex() const
{
    const int row = _table->currentRow();
    if (row < 0 || !_table->item(row, 0))
        return -1;
    return _table->item(row, 0)->data(Qt::UserRole).toInt();
}

void EBWebBookmarksDialog::openSelected()
{
    const int index = selectedIndex();
    if (index < 0 || index >= _bookmarks->entries().size())
        return;
    emit openRequested(_bookmarks->entries().at(index).url);
    accept();
}

void EBWebBookmarksDialog::editSelected()
{
    const int index = selectedIndex();
    if (index < 0 || index >= _bookmarks->entries().size())
        return;
    const EBWebBookmarks::Entry entry = _bookmarks->entries().at(index);
    bool accepted = false;
    const QString title = QInputDialog::getText(this, tr("编辑书签"),
        tr("名称"), QLineEdit::Normal, entry.title, &accepted);
    if (!accepted)
        return;
    const QString folder = QInputDialog::getText(this, tr("编辑书签"),
        tr("分组（可留空）"), QLineEdit::Normal, entry.folder, &accepted);
    if (accepted)
        _bookmarks->update(index, title, folder);
}

void EBWebBookmarksDialog::removeSelected()
{
    _bookmarks->remove(selectedIndex());
}
