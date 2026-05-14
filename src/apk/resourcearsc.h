#ifndef RESOURCEARSC_H
#define RESOURCEARSC_H

#include <QColor>
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

    struct Color {
        QString key;
        QColor color;
        QStringList qualifiers;
    };

    struct File {
        QString key;
        QString path;
        QStringList qualifiers;
    };

    struct Table {
        QList<Alias> aliases;
        QList<Color> colors;
        QList<File> files;
    };

    static Table readTable(const QString &filePath);
    static QList<Alias> readReferenceAliases(const QString &filePath);
};

#endif // RESOURCEARSC_H
