#ifndef RESOURCERESOLVER_H
#define RESOURCERESOLVER_H

#include "icon.h"
#include "resourceref.h"
#include <QColor>
#include <QFileInfo>
#include <QMap>
#include <QStringList>

class ResourceResolver
{
public:
    struct Candidate {
        QString filePath;
        QString dirName;
        QStringList qualifiers;
        QString extension;
        Icon::Type type = Icon::Unknown;
        bool isBitmap = false;
        bool isXml = false;
    };

    struct Value {
        QString filePath;
        QColor color;
        bool found = false;
        bool isBitmap = false;
        bool isXml = false;
        Icon::Type type = Icon::Unknown;
    };

    explicit ResourceResolver(const QString &contentsPath);

    QList<Candidate> candidates(const ResourceRef &ref) const;
    QList<Candidate> bitmapCandidatesByName(const QString &name) const;
    Value resolveBest(const ResourceRef &ref, Icon::Type preferredType) const;
    Value resolveXml(const ResourceRef &ref) const;
    Value resolveBitmap(const ResourceRef &ref, Icon::Type preferredType) const;
    Value resolveColor(const ResourceRef &ref) const;
    ResourceRef resolveAlias(const ResourceRef &ref, int depth = 0) const;

    static bool isBitmapExtension(const QString &extension);
    static Icon::Type typeFromQualifiers(const QStringList &qualifiers);
    static QString qualifierForType(Icon::Type type);

private:
    void loadValues();
    void parseValuesFile(const QString &filePath);
    QList<Candidate> fileCandidates(const ResourceRef &ref) const;
    int score(const Candidate &candidate, Icon::Type preferredType) const;
    int rankForType(Icon::Type type) const;

    QString contentsPath;
    QMap<QString, QColor> colors;
    QMap<QString, QString> aliases;
};

#endif // RESOURCERESOLVER_H
