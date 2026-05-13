#ifndef RESOURCEARSC_H
#define RESOURCEARSC_H

#include <QMap>
#include <QString>

class ResourceArsc
{
public:
    static QMap<QString, QString> readReferenceAliases(const QString &filePath);
};

#endif // RESOURCEARSC_H
