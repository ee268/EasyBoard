#include "ebpagesizedialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QtMath>

EBPageSizeDialog::EBPageSizeDialog(const QSizeF &current, QWidget *parent)
    : QDialog(parent)
    , _width(new QSpinBox(this))
    , _height(new QSpinBox(this))
{
    setObjectName(QStringLiteral("pageSizeDialog"));
    setWindowTitle(tr("自定义页面尺寸"));
    QVBoxLayout *layout = new QVBoxLayout(this);
    QGroupBox *group = new QGroupBox(tr("当前页面"), this);
    QFormLayout *form = new QFormLayout(group);
    _width->setObjectName(QStringLiteral("pageWidthSpinBox"));
    _height->setObjectName(QStringLiteral("pageHeightSpinBox"));
    for (QSpinBox *spin : {_width, _height}) {
        spin->setRange(100, 5000);
        spin->setSuffix(tr(" 像素"));
    }
    _width->setValue(qRound(current.width()));
    _height->setValue(qRound(current.height()));
    form->addRow(tr("宽度："), _width);
    form->addRow(tr("高度："), _height);
    layout->addWidget(group);
    QDialogButtonBox *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

QSizeF EBPageSizeDialog::pageSize() const
{
    return QSizeF(_width->value(), _height->value());
}
