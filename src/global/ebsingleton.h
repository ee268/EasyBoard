#ifndef EBSINGLETON_H
#define EBSINGLETON_H

#include <QMutex>
#include <QPointer>
#include <QMutexLocker>

template <typename T>
class EBSingleton
{
protected:
    EBSingleton() = default;
    EBSingleton(const EBSingleton&) = delete;
    EBSingleton& operator=(const EBSingleton&) = delete;

    static QPointer<T> _instance;

public:
    static QPointer<T> getInstance()
    {
        static QMutex mutex;
        QMutexLocker lock(&mutex);

        if (!_instance)
            _instance = new T;

        return _instance;
    };

    ~EBSingleton() = default;
};

template <typename T>
QPointer<T> EBSingleton<T>::_instance = nullptr;

#endif // EBSINGLETON_H
