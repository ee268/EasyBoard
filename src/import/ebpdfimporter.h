#ifndef EBPDFIMPORTER_H
#define EBPDFIMPORTER_H

#include <QString>
#include <atomic>
#include <functional>

class EBDocument;

class EBPDFImporter
{
public:
    struct Options {
        Options(int first = 1, int last = 0, double renderScale = 1.0)
            : firstPage(first), lastPage(last), scale(renderScale) {}
        int firstPage;
        int lastPage;
        double scale;
    };

    static bool inspectFile(const QString &path, int *pageCount,
                            QString *error);
    static bool importFile(const QString &path, EBDocument *document,
                           QString *error,
                           const std::function<void(int, int)> &progress = {},
                           const std::atomic_bool *cancelled = nullptr,
                           const Options &options = Options());
};

#endif
