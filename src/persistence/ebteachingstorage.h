#ifndef EBTEACHINGSTORAGE_H
#define EBTEACHINGSTORAGE_H

#include <QJsonArray>
#include <QSizeF>

#include "../domain/ebpage.h"

// 教具状态的 JSON 格式独立于文档主体，避免扩大页面序列化文件。
class EBTeachingStorage
{
public:
    static QJsonArray toJson(const EBPage::TeachingTools &tools);
    static bool fromJson(const QJsonValue &value, EBPage::TeachingTools *tools,
                         const QSizeF &pageSize);
};

#endif
