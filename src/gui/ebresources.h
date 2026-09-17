#ifndef EBRESOURCES_H
#define EBRESOURCES_H

#include <QObject>
#include <QPointer>
#include <QIcon>

class EBResources : public QObject
{
    Q_OBJECT
public:
    static EBResources* resources();

    QIcon appIcon() const;

private:
    explicit EBResources(QObject *parent = nullptr);

    static QPointer<EBResources> _instance;
    QIcon _appIcon;
};

#endif // EBRESOURCES_H
