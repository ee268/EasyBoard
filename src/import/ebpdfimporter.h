#ifndef EBPDFIMPORTER_H
#define EBPDFIMPORTER_H

#include <QString>
#include <atomic>
#include <functional>

class EBDocument;

class EBPDFImporter
{
public:
    static bool importFile(const QString &path, EBDocument *document,
                           QString *error,
                           const std::function<void(int, int)> &progress = {},
                           const std::atomic_bool *cancelled = nullptr);
};

#endif
