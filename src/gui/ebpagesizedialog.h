#ifndef EBPAGESIZEDIALOG_H
#define EBPAGESIZEDIALOG_H

#include <QDialog>
#include <QSizeF>

class QSpinBox;

class EBPageSizeDialog : public QDialog
{
public:
    explicit EBPageSizeDialog(const QSizeF &current, QWidget *parent = nullptr);
    QSizeF pageSize() const;

private:
    QSpinBox *_width;
    QSpinBox *_height;
};

#endif // EBPAGESIZEDIALOG_H
