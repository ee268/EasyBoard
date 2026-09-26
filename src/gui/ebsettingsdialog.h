#ifndef EBSETTINGSDIALOG_H
#define EBSETTINGSDIALOG_H

#include <QColor>
#include <QDialog>
#include <QSizeF>
#include <QString>

class QDoubleSpinBox;
class QLineEdit;
class QPushButton;

class EBSettingsDialog : public QDialog
{
public:
    struct Values {
        QString exportDirectory;
        QString downloadDirectory;
        QColor penColor;
        QColor markerColor;
        qreal penWidth;
        qreal markerWidth;
        QSizeF pageSize;
    };

    explicit EBSettingsDialog(QWidget *parent = nullptr);
    Values values() const;

protected:
    void accept() override;

private:
    void chooseColor(QColor *color, QPushButton *button, bool alpha);
    void refreshColorButton(QPushButton *button, const QColor &color);
    void restoreDefaults();

    QLineEdit *_exportDirectory;
    QLineEdit *_downloadDirectory;
    QPushButton *_penColorButton;
    QPushButton *_markerColorButton;
    QDoubleSpinBox *_penWidth;
    QDoubleSpinBox *_markerWidth;
    QDoubleSpinBox *_pageWidth;
    QDoubleSpinBox *_pageHeight;
    QColor _penColor;
    QColor _markerColor;
};

#endif // EBSETTINGSDIALOG_H
