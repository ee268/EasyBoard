#ifndef EBWEBSESSION_H
#define EBWEBSESSION_H

#include <QStringList>

struct EBWebSession
{
    QStringList addresses;
    int currentIndex = 0;

    static EBWebSession load();
    void save() const;
};

#endif
