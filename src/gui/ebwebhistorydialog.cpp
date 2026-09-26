#include "ebwebhistorydialog.h"

#include <QDateTime>
#include <QEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include "ebwebhistory.h"

EBWebHistoryDialog::EBWebHistoryDialog(EBWebHistory *history, QWidget *parent)
    : QDialog(parent)
    , _history(history)
    , _search(new QLineEdit(this))
    , _table(new QTableWidget(this))
    , _clear(new QPushButton(tr("清空历史"), this))
    , _open(new QPushButton(tr("打开"), this))
    , _close(new QPushButton(tr("关闭"), this))
{
    setWindowTitle(tr("浏览历史"));
    resize(760, 480);
    QVBoxLayout *layout = new QVBoxLayout(this);
    _search->setPlaceholderText(tr("搜索标题或网址"));
    layout->addWidget(_search);
    _table->setColumnCount(3);
    _table->setHorizontalHeaderLabels({tr("标题"), tr("网址"), tr("访问时间")});
    _table->horizontalHeader()->setStretchLastSection(false);
    _table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    _table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    _table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    _table->setSelectionBehavior(QAbstractItemView::SelectRows);
    _table->setSelectionMode(QAbstractItemView::SingleSelection);
    _table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(_table);
    QHBoxLayout *actions = new QHBoxLayout;
    actions->addWidget(_clear);
    actions->addStretch();
    actions->addWidget(_open);
    actions->addWidget(_close);
    layout->addLayout(actions);
    connect(_search, &QLineEdit::textChanged,
            this, &EBWebHistoryDialog::refresh);
    connect(_history, &EBWebHistory::changed,
            this, &EBWebHistoryDialog::refresh);
    connect(_table, &QTableWidget::cellDoubleClicked,
            this, &EBWebHistoryDialog::openSelected);
    connect(_open, &QPushButton::clicked,
            this, &EBWebHistoryDialog::openSelected);
    connect(_close, &QPushButton::clicked,
            this, &QDialog::reject);
    connect(_clear, &QPushButton::clicked, this, [this]() {
        if (QMessageBox::question(this, tr("清空历史"),
                                  tr("确定清空全部浏览历史吗？")) == QMessageBox::Yes)
            _history->clear();
    });
    refresh();
}

void EBWebHistoryDialog::changeEvent(QEvent *event)
{
    QDialog::changeEvent(event);
    if (event->type() == QEvent::LanguageChange)
        retranslate();
}

void EBWebHistoryDialog::retranslate()
{
    setWindowTitle(tr("浏览历史"));
    _search->setPlaceholderText(tr("搜索标题或网址"));
    _table->setHorizontalHeaderLabels({tr("标题"), tr("网址"), tr("访问时间")});
    _clear->setText(tr("清空历史"));
    _open->setText(tr("打开"));
    _close->setText(tr("关闭"));
}

void EBWebHistoryDialog::refresh()
{
    _table->setRowCount(0);
    const QString query = _search->text().trimmed();
    for (const EBWebHistory::Entry &entry : _history->entries()) {
        const QString address = entry.url.toString();
        if (!query.isEmpty() && !entry.title.contains(query, Qt::CaseInsensitive)
            && !address.contains(query, Qt::CaseInsensitive))
            continue;
        const int row = _table->rowCount();
        _table->insertRow(row);
        QTableWidgetItem *title = new QTableWidgetItem(
            entry.title.isEmpty() ? address : entry.title);
        title->setData(Qt::UserRole, entry.url);
        _table->setItem(row, 0, title);
        _table->setItem(row, 1, new QTableWidgetItem(address));
        _table->setItem(row, 2, new QTableWidgetItem(
            entry.visited.toString(QStringLiteral("yyyy-MM-dd HH:mm"))));
    }
}

void EBWebHistoryDialog::openSelected()
{
    const int row = _table->currentRow();
    if (row < 0 || !_table->item(row, 0))
        return;
    emit openRequested(_table->item(row, 0)->data(Qt::UserRole).toUrl());
    accept();
}
