#ifndef EBSETTINGS_H
#define EBSETTINGS_H

#include <QString>
#include <QSizeF>
#include <QObject>
#include <QPointer>
#include <QColor>

class QSettings;

class EBSettings : public QObject
{
public:
    static EBSettings* settings();

    static void destroy();

    static QString userDataDir();

    static QString courseDataDir();

    static QString logDir();

    QByteArray windowGeometry() const;

    void setWindowGeometry(const QByteArray& geometry);

    QString lastDocumentPath() const;

    void setLastDocumentPath(const QString &path);

    QString exportDirectory() const;
    bool setExportDirectory(const QString &directory);
    QString downloadDirectory() const;
    bool setDownloadDirectory(const QString &directory);
    QSizeF defaultPageSize() const;
    bool setDefaultPageSize(const QSizeF &size);
    static bool isValidPageSize(const QSizeF &size);

    bool snapEnabled() const;
    void setSnapEnabled(bool enabled);
    bool gridSnapEnabled() const;
    void setGridSnapEnabled(bool enabled);
    QColor penColor() const;
    void setPenColor(const QColor &color);
    QColor markerColor() const;
    void setMarkerColor(const QColor &color);
    qreal penWidth() const;
    void setPenWidth(qreal width);
    qreal markerWidth() const;
    void setMarkerWidth(qreal width);

    bool save();

private:
    explicit EBSettings(QObject *parent = nullptr);

    static QPointer<EBSettings> _instance;

    QSettings* _userSettings;
};

#endif // EBSETTINGS_H
