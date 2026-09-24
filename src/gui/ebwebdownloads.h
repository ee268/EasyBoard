#ifndef EBWEBDOWNLOADS_H
#define EBWEBDOWNLOADS_H

#include <QObject>

class QWebEngineProfile;
class QWidget;

class EBWebDownloads : public QObject
{
    Q_OBJECT
public:
    EBWebDownloads(QWebEngineProfile *profile, QWidget *window);

signals:
    void statusMessage(const QString &message);

private:
    QWidget *_window;
};

#endif
