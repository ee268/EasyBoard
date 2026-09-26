#include "ebsettingsdialog.h"

#include <QColorDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLineEdit>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QStandardPaths>
#include <QTabWidget>
#include <QVBoxLayout>

#include "../core/ebsettings.h"

namespace {
QString defaultDirectory(QStandardPaths::StandardLocation location)
{
    const QString directory = QStandardPaths::writableLocation(location);
    return !directory.isEmpty() && QDir(directory).exists()
        ? QDir(directory).absolutePath() : QDir::homePath();
}
}

EBSettingsDialog::EBSettingsDialog(QWidget *parent)
    : QDialog(parent)
    , _exportDirectory(new QLineEdit(this))
    , _downloadDirectory(new QLineEdit(this))
    , _penColorButton(new QPushButton(this))
    , _markerColorButton(new QPushButton(this))
    , _penWidth(new QDoubleSpinBox(this))
    , _markerWidth(new QDoubleSpinBox(this))
    , _pageWidth(new QDoubleSpinBox(this))
    , _pageHeight(new QDoubleSpinBox(this))
{
    setObjectName(QStringLiteral("settingsDialog"));
    setWindowTitle(tr("设置"));
    setMinimumWidth(500);
    QVBoxLayout *layout = new QVBoxLayout(this);
    QTabWidget *tabs = new QTabWidget(this);
    tabs->setObjectName(QStringLiteral("settingsTabs"));
    layout->addWidget(tabs);

    QWidget *filesTab = new QWidget(tabs);
    QVBoxLayout *filesLayout = new QVBoxLayout(filesTab);
    QGroupBox *exportGroup = new QGroupBox(tr("文件导出"), filesTab);
    QFormLayout *exportForm = new QFormLayout(exportGroup);
    QWidget *exportRow = new QWidget(exportGroup);
    QHBoxLayout *exportRowLayout = new QHBoxLayout(exportRow);
    exportRowLayout->setContentsMargins(0, 0, 0, 0);
    _exportDirectory->setObjectName(QStringLiteral("exportDirectoryEdit"));
    QPushButton *chooseExport = new QPushButton(tr("浏览..."), exportRow);
    exportRowLayout->addWidget(_exportDirectory, 1);
    exportRowLayout->addWidget(chooseExport);
    exportForm->addRow(tr("默认保存位置："), exportRow);
    filesLayout->addWidget(exportGroup);

    QGroupBox *downloadGroup = new QGroupBox(tr("网页下载"), filesTab);
    QFormLayout *downloadForm = new QFormLayout(downloadGroup);
    QWidget *downloadRow = new QWidget(downloadGroup);
    QHBoxLayout *downloadRowLayout = new QHBoxLayout(downloadRow);
    downloadRowLayout->setContentsMargins(0, 0, 0, 0);
    _downloadDirectory->setObjectName(QStringLiteral("downloadDirectoryEdit"));
    QPushButton *chooseDownload = new QPushButton(tr("浏览..."), downloadRow);
    downloadRowLayout->addWidget(_downloadDirectory, 1);
    downloadRowLayout->addWidget(chooseDownload);
    downloadForm->addRow(tr("默认保存位置："), downloadRow);
    filesLayout->addWidget(downloadGroup);
    filesLayout->addStretch();
    tabs->addTab(filesTab, tr("文件与下载"));

    QWidget *brushTab = new QWidget(tabs);
    QVBoxLayout *brushLayout = new QVBoxLayout(brushTab);
    QGroupBox *penGroup = new QGroupBox(tr("画笔"), brushTab);
    QFormLayout *penForm = new QFormLayout(penGroup);
    _penColorButton->setObjectName(QStringLiteral("penColorButton"));
    _penWidth->setObjectName(QStringLiteral("penWidthSpinBox"));
    _penWidth->setRange(1.0, 24.0);
    _penWidth->setDecimals(1);
    _penWidth->setSuffix(tr(" 像素"));
    penForm->addRow(tr("颜色："), _penColorButton);
    penForm->addRow(tr("粗细："), _penWidth);
    brushLayout->addWidget(penGroup);
    QGroupBox *markerGroup = new QGroupBox(tr("荧光笔"), brushTab);
    QFormLayout *markerForm = new QFormLayout(markerGroup);
    _markerColorButton->setObjectName(QStringLiteral("markerColorButton"));
    _markerWidth->setObjectName(QStringLiteral("markerWidthSpinBox"));
    _markerWidth->setRange(4.0, 48.0);
    _markerWidth->setDecimals(1);
    _markerWidth->setSuffix(tr(" 像素"));
    markerForm->addRow(tr("颜色："), _markerColorButton);
    markerForm->addRow(tr("粗细："), _markerWidth);
    brushLayout->addWidget(markerGroup);
    brushLayout->addStretch();
    tabs->addTab(brushTab, tr("画笔"));

    QWidget *pageTab = new QWidget(tabs);
    QVBoxLayout *pageLayout = new QVBoxLayout(pageTab);
    QGroupBox *pageGroup = new QGroupBox(tr("新文档与新页面"), pageTab);
    QFormLayout *pageForm = new QFormLayout(pageGroup);
    _pageWidth->setObjectName(QStringLiteral("defaultPageWidthSpinBox"));
    _pageHeight->setObjectName(QStringLiteral("defaultPageHeightSpinBox"));
    for (QDoubleSpinBox *spin : {_pageWidth, _pageHeight}) {
        spin->setRange(100.0, 5000.0);
        spin->setDecimals(0);
        spin->setSuffix(tr(" 像素"));
    }
    pageForm->addRow(tr("默认宽度："), _pageWidth);
    pageForm->addRow(tr("默认高度："), _pageHeight);
    pageLayout->addWidget(pageGroup);
    pageLayout->addStretch();
    tabs->addTab(pageTab, tr("画布"));

    QDialogButtonBox *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel
        | QDialogButtonBox::RestoreDefaults, this);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &EBSettingsDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &EBSettingsDialog::reject);
    connect(buttons->button(QDialogButtonBox::RestoreDefaults),
            &QPushButton::clicked, this, &EBSettingsDialog::restoreDefaults);
    connect(chooseExport, &QPushButton::clicked, this, [this]() {
        const QString directory = QFileDialog::getExistingDirectory(
            this, tr("选择默认导出目录"), _exportDirectory->text());
        if (!directory.isEmpty())
            _exportDirectory->setText(directory);
    });
    connect(chooseDownload, &QPushButton::clicked, this, [this]() {
        const QString directory = QFileDialog::getExistingDirectory(
            this, tr("选择默认下载目录"), _downloadDirectory->text());
        if (!directory.isEmpty())
            _downloadDirectory->setText(directory);
    });
    connect(_penColorButton, &QPushButton::clicked, this, [this]() {
        chooseColor(&_penColor, _penColorButton, false);
    });
    connect(_markerColorButton, &QPushButton::clicked, this, [this]() {
        chooseColor(&_markerColor, _markerColorButton, true);
    });

    EBSettings *settings = EBSettings::settings();
    _exportDirectory->setText(settings->exportDirectory());
    _downloadDirectory->setText(settings->downloadDirectory());
    _penColor = settings->penColor();
    _markerColor = settings->markerColor();
    _penWidth->setValue(settings->penWidth());
    _markerWidth->setValue(settings->markerWidth());
    _pageWidth->setValue(settings->defaultPageSize().width());
    _pageHeight->setValue(settings->defaultPageSize().height());
    refreshColorButton(_penColorButton, _penColor);
    refreshColorButton(_markerColorButton, _markerColor);
}

EBSettingsDialog::Values EBSettingsDialog::values() const
{
    return {_exportDirectory->text().trimmed(),
            _downloadDirectory->text().trimmed(),
            _penColor, _markerColor, _penWidth->value(),
            _markerWidth->value(),
            QSizeF(_pageWidth->value(), _pageHeight->value())};
}

void EBSettingsDialog::accept()
{
    for (QLineEdit *edit : {_exportDirectory, _downloadDirectory}) {
        const QString directory = edit->text().trimmed();
        if (!QDir::isAbsolutePath(directory) || !QDir(directory).exists()) {
            QMessageBox::warning(this, tr("设置"),
                                 tr("请选择已存在的绝对目录。"));
            edit->setFocus();
            return;
        }
    }
    QDialog::accept();
}

void EBSettingsDialog::chooseColor(QColor *color, QPushButton *button, bool alpha)
{
    const QColor chosen = QColorDialog::getColor(
        *color, this, tr("选择颜色"),
        alpha ? QColorDialog::ShowAlphaChannel : QColorDialog::ColorDialogOptions());
    if (!chosen.isValid())
        return;
    *color = chosen;
    refreshColorButton(button, chosen);
}

void EBSettingsDialog::refreshColorButton(QPushButton *button, const QColor &color)
{
    QPixmap swatch(20, 20);
    swatch.fill(color);
    button->setIcon(QIcon(swatch));
    button->setText(color.name(QColor::HexArgb));
}

void EBSettingsDialog::restoreDefaults()
{
    _exportDirectory->setText(defaultDirectory(QStandardPaths::DocumentsLocation));
    _downloadDirectory->setText(defaultDirectory(QStandardPaths::DownloadLocation));
    _penColor = QColor(0x22, 0x2E, 0x40);
    _markerColor = QColor(0xFF, 0xB8, 0x33, 0x6E);
    _penWidth->setValue(3.0);
    _markerWidth->setValue(18.0);
    _pageWidth->setValue(1200.0);
    _pageHeight->setValue(900.0);
    refreshColorButton(_penColorButton, _penColor);
    refreshColorButton(_markerColorButton, _markerColor);
}
