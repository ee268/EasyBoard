#ifndef EBICONS_H
#define EBICONS_H

#include <QIcon>

inline QIcon ebToolbarIcon(const char *name)
{
    return QIcon(QStringLiteral(":/toolbar/icons/toolbar/")
                 + QLatin1String(name) + QStringLiteral(".svg"));
}

#endif
