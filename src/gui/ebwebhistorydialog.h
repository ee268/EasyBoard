#ifndef EBWEBHISTORYDIALOG_H
#define EBWEBHISTORYDIALOG_H

#include <QDialog>
#include <QUrl>

class EBWebHistory;
class QLineEdit;
class QPushButton;
class QEvent;
class QTableWidget;

class EBWebHistoryDialog : public QDialog
{
    Q_OBJECT
public:
    explicit EBWebHistoryDialog(EBWebHistory *history,
                                QWidget *parent = nullptr);

protected:
    void changeEvent(QEvent *event) override;

signals:
    void openRequested(const QUrl &url);

private:
    void refresh();
    void retranslate();
    void openSelected();

    EBWebHistory *_history;
    QLineEdit *_search;
    QTableWidget *_table;
    QPushButton *_clear;
    QPushButton *_open;
    QPushButton *_close;
};

#endif
