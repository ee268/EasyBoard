#ifndef EBDOCUMENTLIBRARY_H
#define EBDOCUMENTLIBRARY_H

#include <QWidget>

class QListWidget;
class QPushButton;
class QTabWidget;

// 文档工作区只展示存储条目并转发操作意图，文件状态由存储层维护。
class EBDocumentLibrary : public QWidget
{
    Q_OBJECT

public:
    explicit EBDocumentLibrary(QWidget *parent = nullptr);

    void refresh(const QString &currentDocumentId = QString());

signals:
    void openRequested(const QString &path);
    void renameRequested(const QString &path, const QString &title);
    void moveToTrashRequested(const QString &path);
    void restoreRequested(const QString &path);
    void deleteRequested(const QString &path);

private:
    QString selectedPath(QListWidget *list) const;
    void updateButtons();

    QTabWidget *_tabs;
    QListWidget *_documents;
    QListWidget *_trash;
    QPushButton *_openButton;
    QPushButton *_renameButton;
    QPushButton *_trashButton;
    QPushButton *_restoreButton;
    QPushButton *_deleteButton;
};

#endif
