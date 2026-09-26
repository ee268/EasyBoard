#ifndef EBWEBDOWNLOADSDIALOG_H
#define EBWEBDOWNLOADSDIALOG_H

#include <QDialog>

class EBWebDownloads;
class QPushButton;
class QEvent;
class QTableWidget;

class EBWebDownloadsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit EBWebDownloadsDialog(EBWebDownloads *downloads,
                                  QWidget *parent = nullptr);

protected:
    void changeEvent(QEvent *event) override;

private:
    void updateRow(int index);
    void retranslate();
    void refreshRows();
    void updateActions();
    void openFile();
    void openFolder();
    void relinkFile();

    EBWebDownloads *_downloads;
    QTableWidget *_table;
    QPushButton *_cancel;
    QPushButton *_open;
    QPushButton *_folder;
    QPushButton *_relink;
    QPushButton *_remove;
    QPushButton *_clear;
    QPushButton *_close;
};

#endif
