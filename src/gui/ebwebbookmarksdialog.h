#ifndef EBWEBBOOKMARKSDIALOG_H
#define EBWEBBOOKMARKSDIALOG_H

#include <QDialog>
#include <QUrl>

class EBWebBookmarks;
class QLineEdit;
class QPushButton;
class QEvent;
class QTableWidget;

class EBWebBookmarksDialog : public QDialog
{
    Q_OBJECT
public:
    explicit EBWebBookmarksDialog(EBWebBookmarks *bookmarks,
                                  QWidget *parent = nullptr);

protected:
    void changeEvent(QEvent *event) override;

signals:
    void openRequested(const QUrl &url);

private:
    void refresh();
    void retranslate();
    void openSelected();
    void editSelected();
    void removeSelected();
    int selectedIndex() const;

    EBWebBookmarks *_bookmarks;
    QLineEdit *_search;
    QTableWidget *_table;
    QPushButton *_edit;
    QPushButton *_remove;
    QPushButton *_open;
    QPushButton *_close;
};

#endif
