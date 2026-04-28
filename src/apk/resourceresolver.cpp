#include "resourceresolver.h"
#include <QDir>
#include <QDirIterator>
#include <QDomDocument>
#include <QFile>
#include <QRegularExpression>
#include <QTextStream>
#include <QDebug>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QStringConverter>
#endif

static void setUtf8Encoding(QTextStream &stream)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    stream.setEncoding(QStringConverter::Utf8);
#else
    stream.setCodec("UTF-8");
#endif
}

static QString keyForRef(const ResourceRef &ref)
{
    return ref.type() + "/" + ref.name();
}

ResourceResolver::ResourceResolver(const QString &contentsPath)
    : contentsPath(QDir::cleanPath(QDir::fromNativeSeparators(contentsPath)))
{
    loadValues();
}

QList<ResourceResolver::Candidate> ResourceResolver::candidates(const ResourceRef &ref) const
{
    logUnsupportedRef(ref, "resolve resource");
    ResourceRef resolved = resolveAlias(ref);
    if (!resolved.isValid()) {
        resolved = ref;
    }
    return fileCandidates(resolved);
}

QList<ResourceResolver::Candidate> ResourceResolver::bitmapCandidatesByName(const QString &name) const
{
    QList<Candidate> result;
    if (name.isEmpty()) {
        return result;
    }

    const QString resRoot = QDir::cleanPath(contentsPath + "/res");
    QDirIterator dirs(resRoot, QDir::Dirs | QDir::NoDotAndDotDot);
    while (dirs.hasNext()) {
        const QFileInfo dirInfo(dirs.next());
        const QString dirName = dirInfo.fileName();
        const QStringList parts = dirName.split('-');
        const QString resourceType = parts.first();
        if (resourceType != "mipmap" && resourceType != "drawable") {
            continue;
        }

        QDir dir(dirInfo.filePath());
        const QFileInfoList files = dir.entryInfoList(QDir::Files);
        foreach (const QFileInfo &file, files) {
            if (file.completeBaseName() != name || !isBitmapExtension(file.suffix())) {
                continue;
            }

            Candidate candidate;
            candidate.filePath = QDir::cleanPath(file.filePath());
            candidate.dirName = dirName;
            candidate.qualifiers = parts.mid(1);
            candidate.extension = file.suffix().toLower();
            candidate.type = typeFromQualifiers(candidate.qualifiers);
            candidate.isBitmap = true;
            result.append(candidate);
        }
    }

    std::sort(result.begin(), result.end(), [=](const Candidate &a, const Candidate &b) {
        const int rankA = rankForType(a.type);
        const int rankB = rankForType(b.type);
        if (rankA != rankB) {
            return rankA < rankB;
        }
        return a.filePath < b.filePath;
    });
    return result;
}

ResourceResolver::Value ResourceResolver::resolveBest(const ResourceRef &ref, Icon::Type preferredType) const
{
    Value bitmap = resolveBitmap(ref, preferredType);
    if (bitmap.found) {
        return bitmap;
    }

    Value xml = resolveXml(ref);
    if (xml.found) {
        return xml;
    }

    return resolveColor(ref);
}

ResourceResolver::Value ResourceResolver::resolveXml(const ResourceRef &ref) const
{
    QList<Candidate> matches = candidates(ref);
    std::sort(matches.begin(), matches.end(), [=](const Candidate &a, const Candidate &b) {
        const int scoreA = xmlScore(a);
        const int scoreB = xmlScore(b);
        if (scoreA != scoreB) {
            return scoreA < scoreB;
        }
        return a.filePath < b.filePath;
    });

    foreach (const Candidate &candidate, matches) {
        if (candidate.isXml) {
            Value value;
            value.found = true;
            value.isXml = true;
            value.filePath = candidate.filePath;
            value.type = candidate.type;
            return value;
        }
    }
    return Value();
}

ResourceResolver::Value ResourceResolver::resolveBitmap(const ResourceRef &ref, Icon::Type preferredType) const
{
    QList<Candidate> matches = candidates(ref);
    QList<Candidate> bitmaps;
    foreach (const Candidate &candidate, matches) {
        if (candidate.isBitmap) {
            bitmaps.append(candidate);
        }
    }
    if (bitmaps.isEmpty()) {
        return Value();
    }

    std::sort(bitmaps.begin(), bitmaps.end(), [=](const Candidate &a, const Candidate &b) {
        const int scoreA = score(a, preferredType);
        const int scoreB = score(b, preferredType);
        if (scoreA != scoreB) {
            return scoreA < scoreB;
        }
        return rankForType(a.type) > rankForType(b.type);
    });

    Value value;
    value.found = true;
    value.isBitmap = true;
    value.filePath = bitmaps.first().filePath;
    value.type = bitmaps.first().type;
    return value;
}

ResourceResolver::Value ResourceResolver::resolveColor(const ResourceRef &ref) const
{
    ResourceRef resolved = resolveAlias(ref);
    if (!resolved.isValid()) {
        resolved = ref;
    }

    const QString key = keyForRef(resolved);
    if (colors.contains(key)) {
        Value value;
        value.found = true;
        value.color = colors.value(key);
        return value;
    }
    return Value();
}

ResourceRef ResourceResolver::resolveAlias(const ResourceRef &ref, int depth) const
{
    logUnsupportedRef(ref, "resolve alias");
    if (!ref.isValid() || depth > 8) {
        return ResourceRef();
    }

    const QString key = keyForRef(ref);
    if (!aliases.contains(key)) {
        return ResourceRef();
    }

    ResourceRef alias(aliases.value(key));
    ResourceRef next = resolveAlias(alias, depth + 1);
    return next.isValid() ? next : alias;
}

bool ResourceResolver::isBitmapExtension(const QString &extension)
{
    const QString ext = extension.toLower();
    return ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "gif" || ext == "webp";
}

Icon::Type ResourceResolver::typeFromQualifiers(const QStringList &qualifiers)
{
    if (qualifiers.contains("ldpi")) return Icon::Ldpi;
    if (qualifiers.contains("mdpi")) return Icon::Mdpi;
    if (qualifiers.contains("hdpi")) return Icon::Hdpi;
    if (qualifiers.contains("xhdpi")) return Icon::Xhdpi;
    if (qualifiers.contains("xxhdpi")) return Icon::Xxhdpi;
    if (qualifiers.contains("xxxhdpi")) return Icon::Xxxhdpi;
    return Icon::Unknown;
}

QString ResourceResolver::qualifierForType(Icon::Type type)
{
    switch (type) {
        case Icon::Ldpi: return "ldpi";
        case Icon::Mdpi: return "mdpi";
        case Icon::Hdpi: return "hdpi";
        case Icon::Xhdpi: return "xhdpi";
        case Icon::Xxhdpi: return "xxhdpi";
        case Icon::Xxxhdpi: return "xxxhdpi";
        default: return QString();
    }
}

void ResourceResolver::loadValues()
{
    QDirIterator it(QDir::cleanPath(contentsPath + "/res"), QStringList() << "*.xml", QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QFileInfo info(it.next());
        if (info.dir().dirName().split('-').first() == "values") {
            parseValuesFile(info.filePath());
        }
    }
}

void ResourceResolver::parseValuesFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        return;
    }

    QTextStream stream(&file);
    setUtf8Encoding(stream);
    QDomDocument doc;
    if (!doc.setContent(stream.readAll())) {
        return;
    }

    QDomElement resources = doc.firstChildElement("resources");
    QDomElement node = resources.firstChildElement();
    while (!node.isNull()) {
        const QString tag = node.tagName();
        const QString name = node.attribute("name");
        if (!name.isEmpty()) {
            const QString text = node.text().trimmed();
            const QString key = tag + "/" + name;
            if (tag == "color") {
                if (text.startsWith('@')) {
                    aliases.insert(key, text);
                } else {
                    QColor color(text);
                    if (color.isValid()) {
                        colors.insert(key, color);
                    }
                }
            } else if ((tag == "drawable" || tag == "mipmap") && text.startsWith('@')) {
                aliases.insert(key, text);
            } else if (tag == "item") {
                const QString type = node.attribute("type");
                const QString itemKey = type + "/" + name;
                if (!type.isEmpty() && text.startsWith('@')) {
                    aliases.insert(itemKey, text);
                } else if (type == "color") {
                    QColor color(text);
                    if (color.isValid()) {
                        colors.insert(itemKey, color);
                    }
                }
            }
        }
        node = node.nextSiblingElement();
    }
}

QList<ResourceResolver::Candidate> ResourceResolver::fileCandidates(const ResourceRef &ref) const
{
    QList<Candidate> result;
    if (!ref.isValid()) {
        return result;
    }

    const QString resRoot = QDir::cleanPath(contentsPath + "/res");
    QDirIterator dirs(resRoot, QDir::Dirs | QDir::NoDotAndDotDot);
    while (dirs.hasNext()) {
        const QFileInfo dirInfo(dirs.next());
        const QString dirName = dirInfo.fileName();
        const QStringList parts = dirName.split('-');
        if (parts.first() != ref.type()) {
            continue;
        }

        QDir dir(dirInfo.filePath());
        const QFileInfoList files = dir.entryInfoList(QDir::Files);
        foreach (const QFileInfo &file, files) {
            if (file.completeBaseName() != ref.name()) {
                continue;
            }

            Candidate candidate;
            candidate.filePath = QDir::cleanPath(file.filePath());
            candidate.dirName = dirName;
            candidate.qualifiers = parts.mid(1);
            candidate.extension = file.suffix().toLower();
            candidate.type = typeFromQualifiers(candidate.qualifiers);
            candidate.isBitmap = isBitmapExtension(candidate.extension);
            candidate.isXml = candidate.extension == "xml";
            result.append(candidate);
        }
    }
    return result;
}

int ResourceResolver::score(const Candidate &candidate, Icon::Type preferredType) const
{
    int result = qualifierPenalty(candidate.qualifiers);
    const bool nodpi = candidate.qualifiers.contains("nodpi");
    const bool anydpi = candidate.qualifiers.contains("anydpi");

    if (preferredType == Icon::Unknown) {
        if (candidate.type == Icon::Unknown) {
            return result;
        }
        return result + 100 + rankForType(candidate.type);
    }

    if (candidate.type == preferredType) {
        return result;
    }
    if (candidate.type == Icon::Unknown) {
        return result + (nodpi ? 35 : anydpi ? 45 : 100);
    }
    return result + qAbs(rankForType(candidate.type) - rankForType(preferredType)) * 10;
}

int ResourceResolver::xmlScore(const Candidate &candidate) const
{
    int result = qualifierPenalty(candidate.qualifiers);
    if (!candidate.qualifiers.contains("anydpi")) {
        result += 30;
    }
    if (!candidate.qualifiers.contains("v26")) {
        result += 10;
    }
    return result;
}

int ResourceResolver::qualifierPenalty(const QStringList &qualifiers) const
{
    int penalty = 0;
    foreach (const QString &qualifier, qualifiers) {
        if (qualifier == "night") {
            penalty += 1000;
        } else if (qualifier.startsWith("mcc") || qualifier.startsWith("mnc")) {
            penalty += 600;
        } else if (qualifier.length() == 2 || qualifier.startsWith("b+")) {
            penalty += 500;
        } else if (qualifier.startsWith('v')) {
            bool ok = false;
            const int api = qualifier.mid(1).toInt(&ok);
            if (ok && api > 0) {
                penalty += qMax(0, api - 26);
            }
        }
    }
    return penalty;
}

int ResourceResolver::rankForType(Icon::Type type) const
{
    switch (type) {
        case Icon::Ldpi: return 0;
        case Icon::Mdpi: return 1;
        case Icon::Hdpi: return 2;
        case Icon::Xhdpi: return 3;
        case Icon::Xxhdpi: return 4;
        case Icon::Xxxhdpi: return 5;
        default: return -1;
    }
}

void ResourceResolver::logUnsupportedRef(const ResourceRef &ref, const QString &context) const
{
    if (ref.original().isEmpty() || ref.isValid()) {
        return;
    }
    const QString key = context + "|" + ref.original();
    if (loggedUnsupportedRefs.contains(key)) {
        return;
    }
    loggedUnsupportedRefs.insert(key);
    if (ref.isRawId()) {
        qDebug().noquote() << "Unsupported raw resource id while trying to" << context + ":" << ref.original();
    } else if (ref.isFramework()) {
        qDebug().noquote() << "Unsupported Android framework resource while trying to" << context + ":" << ref.original();
    } else if (ref.isAttribute()) {
        qDebug().noquote() << "Unsupported theme attribute resource while trying to" << context + ":" << ref.original();
    } else {
        qDebug().noquote() << "Unsupported resource reference while trying to" << context + ":" << ref.original();
    }
}
