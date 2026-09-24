#include "ebpdfoptionsdialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>

EBPDFOptionsDialog::EBPDFOptionsDialog(int pageCount, QWidget *parent)
    : QDialog(parent)
    , _firstPage(new QSpinBox(this))
    , _lastPage(new QSpinBox(this))
    , _quality(new QComboBox(this))
    , _estimate(new QLabel(this))
{
    setWindowTitle(tr("PDF 导入选项"));
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("PDF 共 %1 页").arg(pageCount), this));
    QFormLayout *form = new QFormLayout;
    _firstPage->setRange(1, pageCount);
    _lastPage->setRange(1, pageCount);
    _lastPage->setValue(pageCount);
    _quality->addItem(tr("标准"), 1.0);
    _quality->addItem(tr("清晰"), 1.5);
    _quality->addItem(tr("高清"), 2.0);
    form->addRow(tr("起始页"), _firstPage);
    form->addRow(tr("结束页"), _lastPage);
    form->addRow(tr("清晰度"), _quality);
    layout->addLayout(form);
    layout->addWidget(_estimate);
    QDialogButtonBox *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);
    connect(_firstPage, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int value) {
        _lastPage->setMinimum(value);
        updateEstimate();
    });
    connect(_lastPage, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &EBPDFOptionsDialog::updateEstimate);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    updateEstimate();
}

EBPDFImporter::Options EBPDFOptionsDialog::options() const
{
    EBPDFImporter::Options result;
    result.firstPage = _firstPage->value();
    result.lastPage = _lastPage->value();
    result.scale = _quality->currentData().toDouble();
    return result;
}

void EBPDFOptionsDialog::updateEstimate()
{
    _estimate->setText(tr("预计导入 %1 页")
                       .arg(_lastPage->value() - _firstPage->value() + 1));
}
