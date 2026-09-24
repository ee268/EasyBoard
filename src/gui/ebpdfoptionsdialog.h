#ifndef EBPDFOPTIONSDIALOG_H
#define EBPDFOPTIONSDIALOG_H

#include <QDialog>

#include "../import/ebpdfimporter.h"

class QComboBox;
class QLabel;
class QSpinBox;

class EBPDFOptionsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit EBPDFOptionsDialog(int pageCount, QWidget *parent = nullptr);
    EBPDFImporter::Options options() const;

private:
    void updateEstimate();

    QSpinBox *_firstPage;
    QSpinBox *_lastPage;
    QComboBox *_quality;
    QLabel *_estimate;
};

#endif
