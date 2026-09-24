#ifndef EBWEBBOOKMARKSDIALOG_H
#define EBWEBBOOKMARKSDIALOG_H

#include <QDialog>
#include <QUrl>

class EBWebBookmarks;
class QLineEdit;
class QTableWidget;

class EBWebBookmarksDialog : public QDialog
{
    Q_OBJECT
public:
    explicit EBWebBookmarksDialog(EBWebBookmarks *bookmarks,
                                  QWidget *parent = nullptr);

signals:
    void openRequested(const QUrl &url);

private:
    void refresh();
    void openSelected();
    void editSelected();
    void removeSelected();
    int selectedIndex() const;

    EBWebBookmarks *_bookmarks;
    QLineEdit *_search;
    QTableWidget *_table;
};

#endif
