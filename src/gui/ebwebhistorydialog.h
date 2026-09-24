#ifndef EBWEBHISTORYDIALOG_H
#define EBWEBHISTORYDIALOG_H

#include <QDialog>
#include <QUrl>

class EBWebHistory;
class QLineEdit;
class QTableWidget;

class EBWebHistoryDialog : public QDialog
{
    Q_OBJECT
public:
    explicit EBWebHistoryDialog(EBWebHistory *history,
                                QWidget *parent = nullptr);

signals:
    void openRequested(const QUrl &url);

private:
    void refresh();
    void openSelected();

    EBWebHistory *_history;
    QLineEdit *_search;
    QTableWidget *_table;
};

#endif
