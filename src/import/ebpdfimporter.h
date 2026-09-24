#ifndef EBPDFIMPORTER_H
#define EBPDFIMPORTER_H

#include <QString>

class EBDocument;

class EBPDFImporter
{
public:
    static bool importFile(const QString &path, EBDocument *document,
                           QString *error);
};

#endif
