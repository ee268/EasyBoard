#ifndef EBIMAGEIMPORTER_H
#define EBIMAGEIMPORTER_H

#include <QString>

#include "../board/ebimageitem.h"

class EBDocument;

// 图片导入器只负责解码和建立新文档，窗口负责切换与保存。
class EBImageImporter
{
public:
    static QString fileDialogFilter();
    static bool importFile(const QString &path, EBDocument *document,
                           QString *error = nullptr);
    static bool loadObject(const QString &path, EBImageItem::State *state,
                           QString *error = nullptr);
};

#endif
