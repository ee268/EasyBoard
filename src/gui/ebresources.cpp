#include "ebresources.h"

#include <QApplication>

QPointer<EBResources> EBResources::_instance = nullptr;

EBResources *EBResources::resources()
{
    if (!_instance) {
        _instance = new EBResources(qApp);
    }

    return _instance;
}

QIcon EBResources::appIcon() const
{
    return _appIcon;
}

EBResources::EBResources(QObject *parent)
    : QObject{parent}
    , _appIcon(QStringLiteral(":/images/easyboard.svg"))
{

}
