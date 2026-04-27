#ifndef ADAPTIVEICON_H
#define ADAPTIVEICON_H

#include "resourceref.h"
#include "resourceresolver.h"
#include <QPixmap>

class AdaptiveIcon
{
public:
    struct Result {
        bool valid = false;
        QPixmap pixmap;
        QString xmlPath;
        QString foregroundPath;
        QString foregroundRef;
        QString backgroundPath;
        QColor backgroundColor;
        Icon::Type type = Icon::Unknown;
    };

    static Result resolve(const ResourceResolver &resolver, const ResourceRef &iconRef, Icon::Type type, const QSize &size);

private:
    static bool parseXml(const QString &xmlPath, ResourceRef *background, ResourceRef *foreground);
    static QPixmap render(const ResourceResolver::Value &background, const ResourceResolver::Value &foreground, const QSize &size, const QPixmap &foregroundOverride = QPixmap());
};

#endif // ADAPTIVEICON_H
