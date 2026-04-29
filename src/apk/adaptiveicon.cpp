#include "adaptiveicon.h"
#include "androidvectorrenderer.h"
#include <QDomDocument>
#include <QFile>
#include <QLinearGradient>
#include <QPainter>
#include <QPen>
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

AdaptiveIcon::Result AdaptiveIcon::resolve(const ResourceResolver &resolver, const ResourceRef &iconRef, Icon::Type type, const QSize &size)
{
    Result result;
    ResourceResolver::Value xml = resolver.resolveXml(iconRef);
    if (!xml.found) {
        return result;
    }

    QFile file(xml.filePath);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        return result;
    }
    QTextStream stream(&file);
    setUtf8Encoding(stream);
    QDomDocument doc;
    if (!doc.setContent(stream.readAll())) {
        return result;
    }
    const QDomElement root = doc.documentElement();
    if (root.tagName() != "adaptive-icon") {
        return result;
    }

    const QDomElement backgroundNode = root.firstChildElement("background");
    const QDomElement foregroundNode = root.firstChildElement("foreground");
    const QDomElement monochromeNode = root.firstChildElement("monochrome");
    ResourceRef backgroundRef(drawableAttr(backgroundNode));
    ResourceRef foregroundRef(drawableAttr(foregroundNode));
    ResourceRef monochromeRef(drawableAttr(monochromeNode));

    QPixmap backgroundPixmap;
    QPixmap foregroundVector;
    QPixmap monochromePixmap;
    ResourceResolver::Value background = backgroundRef.isValid()
            ? resolveLayer(resolver, backgroundRef, type, size, &backgroundPixmap)
            : ResourceResolver::Value();
    ResourceResolver::Value foreground = foregroundRef.isValid()
            ? resolveLayer(resolver, foregroundRef, type, size, &foregroundVector)
            : ResourceResolver::Value();
    ResourceResolver::Value monochrome = monochromeRef.isValid()
            ? resolveLayer(resolver, monochromeRef, type, size, &monochromePixmap)
            : ResourceResolver::Value();
    if (!backgroundNode.isNull() && !backgroundRef.isValid()) {
        backgroundPixmap = renderDrawableElement(resolver, backgroundNode, type, size);
        background.found = !backgroundPixmap.isNull();
        background.isXml = background.found;
    }
    if (!foregroundNode.isNull() && !foregroundRef.isValid()) {
        foregroundVector = renderDrawableElement(resolver, foregroundNode, type, size);
        foreground.found = !foregroundVector.isNull();
        foreground.isXml = foreground.found;
    }
    if (!monochromeNode.isNull() && !monochromeRef.isValid()) {
        monochromePixmap = renderDrawableElement(resolver, monochromeNode, type, size);
        monochrome.found = !monochromePixmap.isNull();
        monochrome.isXml = monochrome.found;
    }
    if (!foreground.found) {
        qDebug() << "Adaptive icon foreground is not renderable:" << (foregroundRef.original().isEmpty() ? QString("inline foreground") : foregroundRef.original());
        return result;
    }

    QPixmap pixmap = render(background, foreground, size, foregroundVector, backgroundPixmap);
    if (pixmap.isNull()) {
        return result;
    }

    result.valid = true;
    result.pixmap = pixmap;
    result.xmlPath = xml.filePath;
    result.descriptor.xmlPath = xml.filePath;
    result.descriptor.foregroundRef = foregroundRef.original();
    result.descriptor.foregroundPath = foreground.isBitmap ? foreground.filePath : QString();
    result.descriptor.backgroundRef = backgroundRef.original();
    result.descriptor.backgroundPath = background.filePath;
    result.descriptor.backgroundColor = background.color;
    result.descriptor.monochromeRef = monochromeRef.original();
    result.descriptor.monochromePath = monochrome.isBitmap ? monochrome.filePath : QString();
    result.descriptor.monochromeColor = monochrome.color;
    result.descriptor.monochromeRenderable = monochrome.found || !monochromePixmap.isNull();
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
        *background = ResourceRef(drawableAttr(backgroundNode));
    }
    if (!foregroundNode.isNull() && foreground) {
        *foreground = ResourceRef(drawableAttr(foregroundNode));
    }
    return true;
}

QPixmap AdaptiveIcon::render(const ResourceResolver::Value &background, const ResourceResolver::Value &foreground, const QSize &size, const QPixmap &foregroundOverride, const QPixmap &backgroundOverride)
{
    if (!size.isValid() || !foreground.found) {
        return QPixmap();
    }

    QImage canvas(size, QImage::Format_ARGB32_Premultiplied);
    canvas.fill(Qt::transparent);
    const QRectF layerRect = adaptiveLayerRect(size);

    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    if (!backgroundOverride.isNull()) {
        painter.drawPixmap(layerRect, backgroundOverride, backgroundOverride.rect());
    } else if (background.color.isValid()) {
        painter.fillRect(canvas.rect(), background.color);
    }

    if (backgroundOverride.isNull() && background.isBitmap && !background.filePath.isEmpty()) {
        QPixmap bg(background.filePath);
        if (!bg.isNull()) {
            painter.drawPixmap(layerRect, bg, bg.rect());
        }
    }

    QPixmap fg = foregroundOverride.isNull() ? QPixmap(foreground.filePath) : foregroundOverride;
    if (!fg.isNull()) {
        painter.drawPixmap(layerRect, fg, fg.rect());
    }

    painter.end();
    return QPixmap::fromImage(canvas);
}

QRectF AdaptiveIcon::adaptiveLayerRect(const QSize &size)
{
    static const qreal extraInsetPercentage = 0.25;
    const qreal insetX = size.width() * extraInsetPercentage;
    const qreal insetY = size.height() * extraInsetPercentage;
    return QRectF(-insetX, -insetY, size.width() + insetX * 2.0, size.height() + insetY * 2.0);
}

ResourceResolver::Value AdaptiveIcon::resolveLayer(const ResourceResolver &resolver, const ResourceRef &ref, Icon::Type type, const QSize &size, QPixmap *pixmap)
{
    ResourceResolver::Value bitmap = resolver.resolveBitmap(ref, type);
    if (bitmap.found) {
        return bitmap;
    }

    ResourceResolver::Value xml = resolver.resolveXml(ref);
    if (xml.found) {
        const QPixmap rendered = renderDrawableXml(resolver, xml.filePath, type, size);
        if (!rendered.isNull()) {
            if (pixmap) {
                *pixmap = rendered;
            }
            xml.isBitmap = false;
            return xml;
        }
    }

    return resolver.resolveColor(ref);
}

QPixmap AdaptiveIcon::renderDrawableXml(const ResourceResolver &resolver, const QString &filePath, Icon::Type type, const QSize &size)
{
    QFile file(filePath);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        return QPixmap();
    }

    QTextStream stream(&file);
    setUtf8Encoding(stream);
    QDomDocument doc;
    if (!doc.setContent(stream.readAll())) {
        return QPixmap();
    }
    return renderDrawableElement(resolver, doc.documentElement(), type, size);
}

QPixmap AdaptiveIcon::renderDrawableElement(const ResourceResolver &resolver, const QDomElement &node, Icon::Type type, const QSize &size)
{
    if (!size.isValid() || node.isNull()) {
        return QPixmap();
    }

    const QString tag = node.tagName();
    if (tag == "vector") {
        return AndroidVectorRenderer::render(resolver, node, size);
    }

    if (tag == "shape") {
        QImage canvas(size, QImage::Format_ARGB32_Premultiplied);
        canvas.fill(Qt::transparent);
        QPainter painter(&canvas);
        painter.setRenderHint(QPainter::Antialiasing);
        const QDomElement solid = node.firstChildElement("solid");
        const QDomElement gradient = node.firstChildElement("gradient");
        const QPainterPath path = shapePath(node, size);
        if (!gradient.isNull()) {
            const QColor start = resolveColorValue(resolver, drawableAttr(gradient).isEmpty() ? gradient.attribute("android:startColor") : drawableAttr(gradient));
            const QColor end = resolveColorValue(resolver, gradient.attribute("android:endColor"));
            QLinearGradient brush(0, size.height(), 0, 0);
            if (gradient.attribute("android:angle").toDouble() == 0.0) {
                brush = QLinearGradient(0, 0, size.width(), 0);
            }
            brush.setColorAt(0, start.isValid() ? start : Qt::transparent);
            brush.setColorAt(1, end.isValid() ? end : start);
            painter.fillPath(path, brush);
        } else if (!solid.isNull()) {
            const QColor color = resolveColorValue(resolver, solid.attribute("android:color"));
            painter.fillPath(path, color.isValid() ? color : Qt::transparent);
        }
        const QDomElement stroke = node.firstChildElement("stroke");
        if (!stroke.isNull()) {
            const QColor strokeColor = resolveColorValue(resolver, stroke.attribute("android:color"));
            const qreal width = dimensionAttr(stroke.attribute("android:width"), 0.0);
            if (strokeColor.isValid() && width > 0.0) {
                QPen pen(strokeColor, width);
                painter.strokePath(path, pen);
            }
        }
        painter.end();
        return QPixmap::fromImage(canvas);
    }

    if (tag == "layer-list") {
        QImage canvas(size, QImage::Format_ARGB32_Premultiplied);
        canvas.fill(Qt::transparent);
        QPainter painter(&canvas);
        QDomElement item = node.firstChildElement("item");
        while (!item.isNull()) {
            QPixmap layer;
            const ResourceRef ref(drawableAttr(item));
            if (ref.isValid()) {
                layer = renderDrawableElement(resolver, item, type, size);
            } else {
                QDomElement child = item.firstChildElement();
                if (!child.isNull()) {
                    layer = renderDrawableElement(resolver, child, type, size);
                }
            }
            if (!layer.isNull()) {
                painter.drawPixmap(canvas.rect(), layer);
            }
            item = item.nextSiblingElement("item");
        }
        painter.end();
        return QPixmap::fromImage(canvas);
    }

    if (tag == "item" || tag == "rotate" || tag == "inset" || tag == "background" || tag == "foreground" || tag == "monochrome") {
        const QString attr = drawableAttr(node);
        const ResourceRef ref(attr);
        QPixmap pixmap;
        if (ref.isValid()) {
            ResourceResolver::Value value = resolveLayer(resolver, ref, type, size, &pixmap);
            if (pixmap.isNull() && value.isBitmap) {
                pixmap = QPixmap(value.filePath);
            } else if (pixmap.isNull() && value.color.isValid()) {
                QImage canvas(size, QImage::Format_ARGB32_Premultiplied);
                canvas.fill(value.color);
                pixmap = QPixmap::fromImage(canvas);
            }
        } else if (attr.startsWith("@android:color/")) {
            QColor color = Qt::transparent;
            const QString name = attr.mid(QString("@android:color/").length());
            if (name == "white") {
                color = Qt::white;
            } else if (name == "black") {
                color = Qt::black;
            } else if (name == "transparent") {
                color = Qt::transparent;
            }
            QImage canvas(size, QImage::Format_ARGB32_Premultiplied);
            canvas.fill(color);
            pixmap = QPixmap::fromImage(canvas);
        } else {
            const QDomElement child = node.firstChildElement();
            if (!child.isNull()) {
                pixmap = renderDrawableElement(resolver, child, type, size);
            }
        }
        if (pixmap.isNull()) {
            return pixmap;
        }
        if (tag == "item" || tag == "inset") {
            const qreal left = dimensionAttr(node.attribute(tag == "inset" ? "android:insetLeft" : "android:left"), 0.0);
            const qreal top = dimensionAttr(node.attribute(tag == "inset" ? "android:insetTop" : "android:top"), 0.0);
            const qreal right = dimensionAttr(node.attribute(tag == "inset" ? "android:insetRight" : "android:right"), 0.0);
            const qreal bottom = dimensionAttr(node.attribute(tag == "inset" ? "android:insetBottom" : "android:bottom"), 0.0);
            if (!qFuzzyIsNull(left) || !qFuzzyIsNull(top) || !qFuzzyIsNull(right) || !qFuzzyIsNull(bottom)) {
                QImage canvas(size, QImage::Format_ARGB32_Premultiplied);
                canvas.fill(Qt::transparent);
                QPainter painter(&canvas);
                painter.setRenderHint(QPainter::SmoothPixmapTransform);
                painter.drawPixmap(QRectF(left, top, size.width() - left - right, size.height() - top - bottom), pixmap, pixmap.rect());
                painter.end();
                pixmap = QPixmap::fromImage(canvas);
            }
        }
        if (tag == "rotate") {
            const qreal degrees = node.attribute("android:fromDegrees").toDouble();
            if (!qFuzzyIsNull(degrees)) {
                const qreal pivotX = percentAttr(node.attribute("android:pivotX"), 0.5) * size.width();
                const qreal pivotY = percentAttr(node.attribute("android:pivotY"), 0.5) * size.height();
                QImage canvas(size, QImage::Format_ARGB32_Premultiplied);
                canvas.fill(Qt::transparent);
                QPainter painter(&canvas);
                painter.setRenderHint(QPainter::SmoothPixmapTransform);
                painter.translate(pivotX, pivotY);
                painter.rotate(degrees);
                painter.translate(-pivotX, -pivotY);
                painter.drawPixmap(canvas.rect(), pixmap);
                painter.end();
                pixmap = QPixmap::fromImage(canvas);
            }
        }
        return pixmap;
    }

    qDebug() << "Adaptive drawable XML tag is not renderable:" << tag;
    return QPixmap();
}

QString AdaptiveIcon::drawableAttr(const QDomElement &node)
{
    QString value = node.attribute("android:drawable");
    if (value.isEmpty()) {
        value = node.attribute("drawable");
    }
    if (value.isEmpty()) {
        const QDomNamedNodeMap attrs = node.attributes();
        for (int i = 0; i < attrs.count(); ++i) {
            const QDomAttr attr = attrs.item(i).toAttr();
            if (attr.name().section(':', -1) == "drawable") {
                value = attr.value();
                break;
            }
        }
    }
    return value.trimmed();
}

QColor AdaptiveIcon::resolveColorValue(const ResourceResolver &resolver, const QString &value)
{
    if (value.startsWith('@')) {
        const ResourceResolver::Value color = resolver.resolveColor(ResourceRef(value));
        if (color.found) {
            return color.color;
        }
    }
    return QColor(value);
}

qreal AdaptiveIcon::dimensionAttr(const QString &value, qreal fallback)
{
    QString text = value.trimmed();
    if (text.isEmpty()) {
        return fallback;
    }

    static const QRegularExpression number("^(-?\\d+(?:\\.\\d+)?)");
    const QRegularExpressionMatch match = number.match(text);
    if (!match.hasMatch()) {
        return fallback;
    }

    bool ok = false;
    const qreal result = match.captured(1).toDouble(&ok);
    return ok ? result : fallback;
}

QPainterPath AdaptiveIcon::shapePath(const QDomElement &node, const QSize &size)
{
    QPainterPath path;
    const QRectF rect(0, 0, size.width(), size.height());
    const QString shape = node.attribute("android:shape", "rectangle");
    if (shape == "oval") {
        path.addEllipse(rect);
        return path;
    }

    const QDomElement corners = node.firstChildElement("corners");
    const qreal radius = dimensionAttr(corners.attribute("android:radius"), 0.0);
    const qreal topLeft = dimensionAttr(corners.attribute("android:topLeftRadius"), radius);
    const qreal topRight = dimensionAttr(corners.attribute("android:topRightRadius"), radius);
    const qreal bottomRight = dimensionAttr(corners.attribute("android:bottomRightRadius"), radius);
    const qreal bottomLeft = dimensionAttr(corners.attribute("android:bottomLeftRadius"), radius);
    if (!qFuzzyIsNull(topLeft - topRight)
            || !qFuzzyIsNull(topLeft - bottomRight)
            || !qFuzzyIsNull(topLeft - bottomLeft)) {
        path.moveTo(rect.left() + topLeft, rect.top());
        path.lineTo(rect.right() - topRight, rect.top());
        path.quadTo(rect.right(), rect.top(), rect.right(), rect.top() + topRight);
        path.lineTo(rect.right(), rect.bottom() - bottomRight);
        path.quadTo(rect.right(), rect.bottom(), rect.right() - bottomRight, rect.bottom());
        path.lineTo(rect.left() + bottomLeft, rect.bottom());
        path.quadTo(rect.left(), rect.bottom(), rect.left(), rect.bottom() - bottomLeft);
        path.lineTo(rect.left(), rect.top() + topLeft);
        path.quadTo(rect.left(), rect.top(), rect.left() + topLeft, rect.top());
        path.closeSubpath();
        return path;
    }

    if (radius > 0.0) {
        path.addRoundedRect(rect, radius, radius);
    } else {
        path.addRect(rect);
    }
    return path;
}

qreal AdaptiveIcon::percentAttr(const QString &value, qreal fallback)
{
    QString text = value.trimmed();
    if (text.endsWith('%')) {
        bool ok = false;
        const qreal percent = text.left(text.length() - 1).toDouble(&ok);
        return ok ? percent / 100.0 : fallback;
    }
    bool ok = false;
    const qreal number = text.toDouble(&ok);
    return ok ? number : fallback;
}
