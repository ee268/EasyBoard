#ifndef EBWEBDOWNLOADSDIALOG_H
#define EBWEBDOWNLOADSDIALOG_H

#include <QDialog>

class EBWebDownloads;
class QPushButton;
class QTableWidget;

class EBWebDownloadsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit EBWebDownloadsDialog(EBWebDownloads *downloads,
                                  QWidget *parent = nullptr);

private:
    void updateRow(int index);
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
};

#endif
