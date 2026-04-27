#include "adaptiveicon.h"
#include "androidvectorrenderer.h"
#include <QDomDocument>
#include <QFile>
#include <QPainter>
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

AdaptiveIcon::Result AdaptiveIcon::resolve(const ResourceResolver &resolver, const ResourceRef &iconRef, Icon::Type type, const QSize &size)
{
    Result result;
    ResourceResolver::Value xml = resolver.resolveXml(iconRef);
    if (!xml.found) {
        return result;
    }

    ResourceRef backgroundRef;
    ResourceRef foregroundRef;
    if (!parseXml(xml.filePath, &backgroundRef, &foregroundRef) || !foregroundRef.isValid()) {
        return result;
    }

    ResourceResolver::Value background = resolver.resolveBest(backgroundRef, type);
    ResourceResolver::Value foreground = resolver.resolveBitmap(foregroundRef, type);
    QPixmap foregroundVector;
    if (!foreground.found) {
        const ResourceResolver::Value foregroundXml = resolver.resolveXml(foregroundRef);
        if (foregroundXml.found) {
            foregroundVector = AndroidVectorRenderer::render(resolver, foregroundXml.filePath, size);
            if (!foregroundVector.isNull()) {
                foreground = foregroundXml;
            }
        }
    }
    if (!foreground.found) {
        qDebug() << "Adaptive icon foreground is not renderable:" << foregroundRef.original();
        return result;
    }

    QPixmap pixmap = render(background, foreground, size, foregroundVector);
    if (pixmap.isNull()) {
        return result;
    }

    result.valid = true;
    result.pixmap = pixmap;
    result.xmlPath = xml.filePath;
    result.foregroundPath = foreground.isBitmap ? foreground.filePath : QString();
    result.foregroundRef = foregroundRef.original();
    result.backgroundPath = background.filePath;
    result.backgroundColor = background.color;
    result.type = type;
    return result;
}

bool AdaptiveIcon::parseXml(const QString &xmlPath, ResourceRef *background, ResourceRef *foreground)
{
    QFile file(xmlPath);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        return false;
    }

    QTextStream stream(&file);
    setUtf8Encoding(stream);
    QDomDocument doc;
    if (!doc.setContent(stream.readAll())) {
        return false;
    }

    QDomElement root = doc.documentElement();
    if (root.tagName() != "adaptive-icon") {
        return false;
    }

    const QDomElement backgroundNode = root.firstChildElement("background");
    const QDomElement foregroundNode = root.firstChildElement("foreground");
    if (!backgroundNode.isNull() && background) {
        *background = ResourceRef(backgroundNode.attribute("android:drawable"));
    }
    if (!foregroundNode.isNull() && foreground) {
        *foreground = ResourceRef(foregroundNode.attribute("android:drawable"));
    }
    return true;
}

QPixmap AdaptiveIcon::render(const ResourceResolver::Value &background, const ResourceResolver::Value &foreground, const QSize &size, const QPixmap &foregroundOverride)
{
    if (!size.isValid() || !foreground.found) {
        return QPixmap();
    }

    QImage canvas(size, QImage::Format_ARGB32_Premultiplied);
    canvas.fill(Qt::transparent);

    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    if (background.color.isValid()) {
        painter.fillRect(canvas.rect(), background.color);
    }

    if (background.isBitmap && !background.filePath.isEmpty()) {
        QPixmap bg(background.filePath);
        if (!bg.isNull()) {
            painter.drawPixmap(canvas.rect(), bg);
        }
    }

    QPixmap fg = foregroundOverride.isNull() ? QPixmap(foreground.filePath) : foregroundOverride;
    if (!fg.isNull()) {
        painter.drawPixmap(canvas.rect(), fg);
    }

    painter.end();
    return QPixmap::fromImage(canvas);
}
