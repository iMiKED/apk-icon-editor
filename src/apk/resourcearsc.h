#ifndef RESOURCEARSC_H
#define RESOURCEARSC_H

#include <QList>
#include <QString>
#include <QStringList>

class ResourceArsc
{
public:
    struct Alias {
        QString key;
        QString value;
        QStringList qualifiers;
    };

    static QList<Alias> readReferenceAliases(const QString &filePath);
};

#endif // RESOURCEARSC_H
