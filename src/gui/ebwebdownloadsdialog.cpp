#include "ebwebdownloadsdialog.h"

#include <QDesktopServices>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include "ebwebdownloads.h"

EBWebDownloadsDialog::EBWebDownloadsDialog(EBWebDownloads *downloads,
                                           QWidget *parent)
    : QDialog(parent)
    , _downloads(downloads)
    , _table(new QTableWidget(this))
    , _cancel(new QPushButton(tr("取消下载"), this))
    , _open(new QPushButton(tr("打开文件"), this))
    , _folder(new QPushButton(tr("打开文件夹"), this))
    , _relink(new QPushButton(tr("重新定位"), this))
    , _remove(new QPushButton(tr("移除记录"), this))
    , _clear(new QPushButton(tr("清空记录"), this))
    , _close(new QPushButton(tr("关闭"), this))
{
    setWindowTitle(tr("网页下载"));
    resize(760, 420);
    QVBoxLayout *layout = new QVBoxLayout(this);
    _table->setColumnCount(4);
    _table->setHorizontalHeaderLabels(
        {tr("文件"), tr("进度"), tr("状态"), tr("保存位置")});
    _table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    _table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    _table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    _table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    _table->setSelectionBehavior(QAbstractItemView::SelectRows);
    _table->setSelectionMode(QAbstractItemView::SingleSelection);
    _table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(_table);
    QHBoxLayout *actions = new QHBoxLayout;
    actions->addWidget(_cancel);
    actions->addWidget(_open);
    actions->addWidget(_folder);
    actions->addWidget(_relink);
    actions->addWidget(_remove);
    actions->addWidget(_clear);
    actions->addStretch();
    actions->addWidget(_close);
    layout->addLayout(actions);
    connect(_downloads, &EBWebDownloads::entryAdded,
            this, &EBWebDownloadsDialog::updateRow);
    connect(_downloads, &EBWebDownloads::entryChanged,
            this, &EBWebDownloadsDialog::updateRow);
    connect(_downloads, &EBWebDownloads::entriesReset,
            this, &EBWebDownloadsDialog::refreshRows);
    connect(_table, &QTableWidget::itemSelectionChanged,
            this, &EBWebDownloadsDialog::updateActions);
    connect(_table, &QTableWidget::cellDoubleClicked,
            this, &EBWebDownloadsDialog::openFile);
    connect(_cancel, &QPushButton::clicked, this, [this]() {
        _downloads->cancel(_table->currentRow());
    });
    connect(_open, &QPushButton::clicked,
            this, &EBWebDownloadsDialog::openFile);
    connect(_folder, &QPushButton::clicked,
            this, &EBWebDownloadsDialog::openFolder);
    connect(_relink, &QPushButton::clicked,
            this, &EBWebDownloadsDialog::relinkFile);
    connect(_remove, &QPushButton::clicked, this, [this]() {
        _downloads->removeRecord(_table->currentRow());
    });
    connect(_clear, &QPushButton::clicked,
            _downloads, &EBWebDownloads::clearFinished);
    connect(_close, &QPushButton::clicked, this, &QDialog::close);
    QTimer *refreshTimer = new QTimer(this);
    refreshTimer->setInterval(3000);
    connect(refreshTimer, &QTimer::timeout,
            _downloads, &EBWebDownloads::refreshFiles);
    refreshTimer->start();
    refreshRows();
}

void EBWebDownloadsDialog::changeEvent(QEvent *event)
{
    QDialog::changeEvent(event);
    if (event->type() == QEvent::LanguageChange)
        retranslate();
}

void EBWebDownloadsDialog::retranslate()
{
    setWindowTitle(tr("网页下载"));
    _table->setHorizontalHeaderLabels(
        {tr("文件"), tr("进度"), tr("状态"), tr("保存位置")});
    _cancel->setText(tr("取消下载"));
    _open->setText(tr("打开文件"));
    _folder->setText(tr("打开文件夹"));
    _relink->setText(tr("重新定位"));
    _remove->setText(tr("移除记录"));
    _clear->setText(tr("清空记录"));
    _close->setText(tr("关闭"));
    for (int index = 0; index < _table->rowCount(); ++index)
        updateRow(index);
}

void EBWebDownloadsDialog::refreshRows()
{
    _table->setRowCount(0);
    for (int index = 0; index < _downloads->count(); ++index)
        updateRow(index);
    updateActions();
}

void EBWebDownloadsDialog::updateRow(int index)
{
    if (index < 0 || index >= _downloads->count())
        return;
    while (_table->rowCount() <= index)
        _table->insertRow(_table->rowCount());
    const EBWebDownloads::Entry &entry = _downloads->entryAt(index);
    const QString progress = entry.total > 0
        ? tr("%1% · %2 / %3 MB")
            .arg(qBound(0, int(entry.received * 100.0 / entry.total), 100))
            .arg(QString::number(entry.received / 1048576.0, 'f', 1))
            .arg(QString::number(entry.total / 1048576.0, 'f', 1))
        : tr("%1 MB").arg(QString::number(entry.received / 1048576.0, 'f', 1));
    const QString values[] = {
        QFileInfo(entry.path).fileName(), progress, _downloads->statusText(entry),
        QFileInfo(entry.path).absolutePath()
    };
    for (int column = 0; column < 4; ++column) {
        QTableWidgetItem *item = _table->item(index, column);
        if (!item) {
            item = new QTableWidgetItem;
            _table->setItem(index, column, item);
        }
        item->setText(values[column]);
    }
    updateActions();
}

void EBWebDownloadsDialog::updateActions()
{
    const int row = _table->currentRow();
    const bool selected = row >= 0 && row < _downloads->count();
    _cancel->setEnabled(selected && _downloads->entryAt(row).running);
    _open->setEnabled(selected && _downloads->entryAt(row).completed
                      && QFileInfo::exists(_downloads->entryAt(row).path));
    _folder->setEnabled(selected && QFileInfo(
        _downloads->entryAt(row).path).dir().exists());
    _relink->setEnabled(selected && _downloads->entryAt(row).completed
                         && !QFileInfo(_downloads->entryAt(row).path).isFile());
    _remove->setEnabled(selected && !_downloads->entryAt(row).running);
    bool hasHistory = false;
    for (int index = 0; index < _downloads->count(); ++index) {
        if (!_downloads->entryAt(index).running) {
            hasHistory = true;
            break;
        }
    }
    _clear->setEnabled(hasHistory);
}

void EBWebDownloadsDialog::relinkFile()
{
    const int row = _table->currentRow();
    if (row < 0 || row >= _downloads->count())
        return;
    const QString path = QFileDialog::getOpenFileName(this,
        tr("定位下载文件"), _downloads->entryAt(row).path);
    if (path.isEmpty())
        return;
    QString error;
    if (!_downloads->relink(row, path, &error))
        QMessageBox::warning(this, tr("定位下载文件"), error);
}

void EBWebDownloadsDialog::openFile()
{
    const int row = _table->currentRow();
    if (row < 0 || row >= _downloads->count())
        return;
    const EBWebDownloads::Entry &entry = _downloads->entryAt(row);
    if (!entry.completed)
        return;
    if (!QFileInfo(entry.path).isFile()) {
        _downloads->refreshFiles();
        QMessageBox::warning(this, tr("打开下载"),
                             tr("文件已移动或删除，请重新定位"));
        return;
    }
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(entry.path)))
        QMessageBox::warning(this, tr("打开下载"), tr("无法打开下载文件"));
}

void EBWebDownloadsDialog::openFolder()
{
    const int row = _table->currentRow();
    if (row < 0 || row >= _downloads->count())
        return;
    const QString directory = QFileInfo(
        _downloads->entryAt(row).path).absolutePath();
    if (!QDir(directory).exists()) {
        QMessageBox::warning(this, tr("打开文件夹"),
                             tr("下载目录已移动或删除"));
        return;
    }
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(directory)))
        QMessageBox::warning(this, tr("打开文件夹"), tr("无法打开下载目录"));
}
