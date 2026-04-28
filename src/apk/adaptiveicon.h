#ifndef ADAPTIVEICON_H
#define ADAPTIVEICON_H

#include "resourceref.h"
#include "resourceresolver.h"
#include <QDomElement>
#include <QPixmap>

class AdaptiveIcon
{
public:
    struct Result {
        bool valid = false;
        QPixmap pixmap;
        QString xmlPath;
        AdaptiveIconDescriptor descriptor;
        Icon::Type type = Icon::Unknown;
    };

    static Result resolve(const ResourceResolver &resolver, const ResourceRef &iconRef, Icon::Type type, const QSize &size);

private:
    static bool parseXml(const QString &xmlPath, ResourceRef *background, ResourceRef *foreground);
    static QPixmap render(const ResourceResolver::Value &background, const ResourceResolver::Value &foreground, const QSize &size, const QPixmap &foregroundOverride = QPixmap(), const QPixmap &backgroundOverride = QPixmap());
    static ResourceResolver::Value resolveLayer(const ResourceResolver &resolver, const ResourceRef &ref, Icon::Type type, const QSize &size, QPixmap *pixmap);
    static QPixmap renderDrawableXml(const ResourceResolver &resolver, const QString &filePath, Icon::Type type, const QSize &size);
    static QPixmap renderDrawableElement(const ResourceResolver &resolver, const QDomElement &node, Icon::Type type, const QSize &size);
    static QString drawableAttr(const QDomElement &node);
    static QColor resolveColorValue(const ResourceResolver &resolver, const QString &value);
    static qreal percentAttr(const QString &value, qreal fallback = 0.5);
};

#endif // ADAPTIVEICON_H
