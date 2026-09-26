#include "ebdocumentlibrary.h"

#include <QFont>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>

#include "../persistence/ebdocumentstorage.h"

namespace {
constexpr int kPathRole = Qt::UserRole;
constexpr int kTitleRole = Qt::UserRole + 1;
constexpr int kPageCountRole = Qt::UserRole + 2;
constexpr int kUpdatedAtRole = Qt::UserRole + 3;
constexpr int kCurrentRole = Qt::UserRole + 4;

void fillList(QListWidget *list, const QVector<EBDocumentSummary> &documents,
              const QString &currentDocumentId)
{
    const QString selected = list->currentItem()
        ? list->currentItem()->data(kPathRole).toString() : QString();
    list->clear();
    int selectedRow = -1;
    for (const EBDocumentSummary &document : documents) {
        const QString details = QObject::tr("%1 页 · %2")
            .arg(document.pageCount)
            .arg(document.updatedAt.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm")));
        QListWidgetItem *item = new QListWidgetItem(
            document.title + QLatin1Char('\n') + details, list);
        item->setData(kPathRole, document.path);
        item->setData(kTitleRole, document.title);
        item->setData(kPageCountRole, document.pageCount);
        item->setData(kUpdatedAtRole, document.updatedAt);
        item->setData(kCurrentRole, document.id == currentDocumentId);
        item->setToolTip(document.path);
        item->setSizeHint(QSize(0, 58));
        if (document.id == currentDocumentId) {
            QFont font = item->font();
            font.setBold(true);
            item->setFont(font);
            item->setText(QObject::tr("当前 · %1\n%2").arg(document.title, details));
        }
        if (document.path == selected)
            selectedRow = list->count() - 1;
    }
    if (selectedRow >= 0)
        list->setCurrentRow(selectedRow);
    else if (list->count() > 0)
        list->setCurrentRow(0);
}
}

EBDocumentLibrary::EBDocumentLibrary(QWidget *parent)
    : QWidget(parent)
    , _tabs(new QTabWidget(this))
    , _documents(new QListWidget(this))
    , _trash(new QListWidget(this))
    , _search(new QLineEdit(this))
    , _newButton(new QPushButton(tr("新建"), this))
    , _duplicateButton(new QPushButton(tr("复制"), this))
    , _openButton(new QPushButton(tr("打开"), this))
    , _renameButton(new QPushButton(tr("重命名"), this))
    , _trashButton(new QPushButton(tr("移入回收站"), this))
    , _restoreButton(new QPushButton(tr("恢复"), this))
    , _deleteButton(new QPushButton(tr("永久删除"), this))
{
    setObjectName(QStringLiteral("documentLibrary"));
    setStyleSheet(QStringLiteral(
        "EBDocumentLibrary { background: #F4F7F8; }"
        "QListWidget { background: white; border: 1px solid #DCE6EA; border-radius: 8px; padding: 5px; outline: none; }"
        "QListWidget::item { border-radius: 6px; padding: 6px 10px; }"
        "QListWidget::item:selected { background: #DDF3EF; color: #193A3A; }"));

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(18, 16, 18, 16);
    QLabel *title = new QLabel(tr("文档管理"), this);
    title->setObjectName(QStringLiteral("documentLibraryTitle"));
    QFont titleFont = title->font();
    titleFont.setPointSize(titleFont.pointSize() + 3);
    titleFont.setBold(true);
    title->setFont(titleFont);
    root->addWidget(title);
    root->addWidget(_tabs, 1);

    QWidget *documentsPage = new QWidget(_tabs);
    QVBoxLayout *documentsLayout = new QVBoxLayout(documentsPage);
    _documents->setObjectName(QStringLiteral("documentList"));
    _documents->setSelectionMode(QAbstractItemView::SingleSelection);
    _search->setObjectName(QStringLiteral("documentSearchEdit"));
    _search->setPlaceholderText(tr("搜索文档名称"));
    documentsLayout->addWidget(_search);
    documentsLayout->addWidget(_documents, 1);
    QHBoxLayout *documentButtons = new QHBoxLayout;
    _openButton->setObjectName(QStringLiteral("openDocumentButton"));
    _newButton->setObjectName(QStringLiteral("newDocumentButton"));
    _duplicateButton->setObjectName(QStringLiteral("duplicateDocumentButton"));
    _renameButton->setObjectName(QStringLiteral("renameDocumentButton"));
    _trashButton->setObjectName(QStringLiteral("trashDocumentButton"));
    documentButtons->addWidget(_newButton);
    documentButtons->addWidget(_openButton);
    documentButtons->addWidget(_duplicateButton);
    documentButtons->addWidget(_renameButton);
    documentButtons->addStretch();
    documentButtons->addWidget(_trashButton);
    documentsLayout->addLayout(documentButtons);
    _tabs->addTab(documentsPage, tr("我的文档"));

    QWidget *trashPage = new QWidget(_tabs);
    QVBoxLayout *trashLayout = new QVBoxLayout(trashPage);
    _trash->setObjectName(QStringLiteral("trashDocumentList"));
    _trash->setSelectionMode(QAbstractItemView::SingleSelection);
    trashLayout->addWidget(_trash, 1);
    QHBoxLayout *trashButtons = new QHBoxLayout;
    _restoreButton->setObjectName(QStringLiteral("restoreDocumentButton"));
    _deleteButton->setObjectName(QStringLiteral("deleteDocumentButton"));
    trashButtons->addWidget(_restoreButton);
    trashButtons->addStretch();
    trashButtons->addWidget(_deleteButton);
    trashLayout->addLayout(trashButtons);
    _tabs->addTab(trashPage, tr("回收站"));

    connect(_documents, &QListWidget::currentRowChanged,
            this, [this]() { updateButtons(); });
    connect(_trash, &QListWidget::currentRowChanged,
            this, [this]() { updateButtons(); });
    connect(_tabs, &QTabWidget::currentChanged,
            this, [this]() { updateButtons(); });
    connect(_search, &QLineEdit::textChanged,
            this, [this]() { applySearch(); });
    connect(_newButton, &QPushButton::clicked,
            this, &EBDocumentLibrary::newRequested);
    connect(_duplicateButton, &QPushButton::clicked, this, [this]() {
        const QString path = selectedPath(_documents);
        if (!path.isEmpty())
            emit duplicateRequested(path);
    });
    connect(_documents, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem *) {
        const QString path = selectedPath(_documents);
        if (!path.isEmpty())
            emit openRequested(path);
    });
    connect(_openButton, &QPushButton::clicked, this, [this]() {
        const QString path = selectedPath(_documents);
        if (!path.isEmpty())
            emit openRequested(path);
    });
    connect(_renameButton, &QPushButton::clicked, this, [this]() {
        QListWidgetItem *item = _documents->currentItem();
        if (!item)
            return;
        bool accepted = false;
        const QString title = QInputDialog::getText(
            this, tr("重命名文档"), tr("文档名称："), QLineEdit::Normal,
            item->data(kTitleRole).toString(), &accepted).trimmed();
        if (accepted && !title.isEmpty())
            emit renameRequested(item->data(kPathRole).toString(), title);
    });
    connect(_trashButton, &QPushButton::clicked, this, [this]() {
        const QString path = selectedPath(_documents);
        if (!path.isEmpty())
            emit moveToTrashRequested(path);
    });
    connect(_restoreButton, &QPushButton::clicked, this, [this]() {
        const QString path = selectedPath(_trash);
        if (!path.isEmpty())
            emit restoreRequested(path);
    });
    connect(_deleteButton, &QPushButton::clicked, this, [this]() {
        const QString path = selectedPath(_trash);
        if (path.isEmpty())
            return;
        if (QMessageBox::question(this, tr("永久删除文档"),
                                  tr("此操作无法撤销，确定永久删除吗？"))
            == QMessageBox::Yes)
            emit deleteRequested(path);
    });
    refresh();
}

void EBDocumentLibrary::retranslate()
{
    findChild<QLabel *>(QStringLiteral("documentLibraryTitle"))->setText(tr("文档管理"));
    _tabs->setTabText(0, tr("我的文档"));
    _tabs->setTabText(1, tr("回收站"));
    _search->setPlaceholderText(tr("搜索文档名称"));
    _newButton->setText(tr("新建"));
    _duplicateButton->setText(tr("复制"));
    _openButton->setText(tr("打开"));
    _renameButton->setText(tr("重命名"));
    _trashButton->setText(tr("移入回收站"));
    _restoreButton->setText(tr("恢复"));
    _deleteButton->setText(tr("永久删除"));
    for (QListWidget *list : {_documents, _trash}) {
        for (int index = 0; index < list->count(); ++index) {
            QListWidgetItem *item = list->item(index);
            const QString details = QObject::tr("%1 页 · %2")
                .arg(item->data(kPageCountRole).toInt())
                .arg(item->data(kUpdatedAtRole).toDateTime().toLocalTime()
                     .toString(QStringLiteral("yyyy-MM-dd HH:mm")));
            const QString title = item->data(kTitleRole).toString();
            item->setText(item->data(kCurrentRole).toBool()
                          ? QObject::tr("当前 · %1\n%2").arg(title, details)
                          : title + QLatin1Char('\n') + details);
        }
    }
}

void EBDocumentLibrary::refresh(const QString &currentDocumentId)
{
    fillList(_documents, EBDocumentStorage::listDocuments(false), currentDocumentId);
    fillList(_trash, EBDocumentStorage::listDocuments(true), QString());
    applySearch();
    updateButtons();
}

void EBDocumentLibrary::applySearch()
{
    const QString query = _search->text().trimmed();
    for (int index = 0; index < _documents->count(); ++index) {
        QListWidgetItem *item = _documents->item(index);
        item->setHidden(!item->data(kTitleRole).toString().contains(
            query, Qt::CaseInsensitive));
    }
    if (!_documents->currentItem()
        || _documents->currentItem()->isHidden()) {
        _documents->clearSelection();
        _documents->setCurrentRow(-1);
        for (int index = 0; index < _documents->count(); ++index) {
            if (!_documents->item(index)->isHidden()) {
                _documents->setCurrentRow(index);
                break;
            }
        }
    }
    updateButtons();
}

QString EBDocumentLibrary::selectedPath(QListWidget *list) const
{
    return list->currentItem() && !list->currentItem()->isHidden()
        ? list->currentItem()->data(kPathRole).toString() : QString();
}

void EBDocumentLibrary::updateButtons()
{
    const bool documentSelected = _documents->currentItem()
        && !_documents->currentItem()->isHidden();
    const bool trashSelected = _trash->currentItem();
    _openButton->setEnabled(documentSelected);
    _duplicateButton->setEnabled(documentSelected);
    _renameButton->setEnabled(documentSelected);
    _trashButton->setEnabled(documentSelected);
    _restoreButton->setEnabled(trashSelected);
    _deleteButton->setEnabled(trashSelected);
}
