#include "icon.h"
#include "globals.h"
#include <QDir>
#include <QDomDocument>
#include <QFile>
#include <QFileInfo>
#include <QPainter>
#include <QLabel>
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

Icon::Icon(QString filename, Type type, Scope scope)
{
    this->type = type;
    this->scope = scope;
    modified = false;
    virtualIcon = false;

    qualifiers = QFileInfo(filename).path().split('/').last().split('-').mid(1);

    // Guess qualifier from icon type:
    if (qualifiers.isEmpty() && type != Unknown) {
        QString dpi;
        switch (type) {
            case Ldpi: dpi = "ldpi"; break;
            case Mdpi: dpi = "mdpi"; break;
            case Hdpi: dpi = "hdpi"; break;
            case Xhdpi: dpi = "xhdpi"; break;
            case Xxhdpi: dpi = "xxhdpi"; break;
            case Xxxhdpi: dpi = "xxxhdpi"; break;
        }
        if (!dpi.isEmpty()) {
            qualifiers.append(dpi);
        }
    }
    // Guess icon type from qualifiers:
    if (type == Unknown && !qualifiers.isEmpty()) {
        foreach (const QString &qualifier, qualifiers) {
            if (qualifier == "ldpi") { this->type = Ldpi; break; }
            else if (qualifier == "mdpi") { this->type = Mdpi; break; }
            else if (qualifier == "hdpi") { this->type = Hdpi; break; }
            else if (qualifier == "xhdpi") { this->type = Xhdpi; break; }
            else if (qualifier == "xxhdpi") { this->type = Xxhdpi; break; }
            else if (qualifier == "xxxhdpi") { this->type = Xxxhdpi; break; }
        }
    }

    load(filename);
}

Icon::Icon(QString filename, const QPixmap &pixmap, const QStringList &saveTargets, Type type, Scope scope)
    : Icon(filename, type, scope)
{
    qualifiers.clear();
    QString dpi;
    switch (type) {
        case Ldpi: dpi = "ldpi"; break;
        case Mdpi: dpi = "mdpi"; break;
        case Hdpi: dpi = "hdpi"; break;
        case Xhdpi: dpi = "xhdpi"; break;
        case Xxhdpi: dpi = "xxhdpi"; break;
        case Xxxhdpi: dpi = "xxxhdpi"; break;
        default: break;
    }
    if (!dpi.isEmpty()) {
        qualifiers.append(dpi);
    }
    this->saveTargets = saveTargets;
    virtualIcon = true;
    originalPixmap = pixmap;
    setPixmap(pixmap);
}

Icon::Icon(QString filename, const QPixmap &pixmap, const QStringList &saveTargets, const AdaptiveIconDescriptor &adaptiveDescriptor, Type type, Scope scope)
    : Icon(filename, pixmap, saveTargets, type, scope)
{
    this->adaptiveDescriptor = adaptiveDescriptor;
}

bool Icon::load(QString filename)
{
    filePath = filename;
    return revert();
}

bool Icon::save(QString filename)
{
    if (pixmap.isNull()) {
        // Don't save an empty icon (but don't throw error).
        return true;
    }

    const bool explicitExport = !filename.isEmpty();
    if (explicitExport) {
        const bool needsFormatHint = QFileInfo(filename).suffix().isEmpty();
        const char *format = needsFormatHint ? "PNG" : NULL;
        if (isAdaptiveIcon()) {
            qDebug().noquote() << "Exporting adaptive icon preview:\n" + getToolTip();
            qDebug().noquote() << "Export file:" << Path::display(filename);
        }
        QDir().mkpath(QFileInfo(filename).absolutePath());
        return getPixmap().save(filename, format, 100);
    }

    if (virtualIcon && saveTargets.isEmpty()) {
        return true;
    }
    if (!saveTargets.isEmpty() && !modified) {
        return true;
    }
    QStringList targets = saveTargets;
    if (targets.isEmpty()) {
        targets.append(filePath);
    }
    if (isAdaptiveIcon()) {
        qDebug().noquote() << "Saving adaptive icon write-back:\n" + getToolTip();
    }
    bool result = true;
    foreach (const QString &target, targets) {
        QDir().mkpath(QFileInfo(target).absolutePath());
        const bool needsFormatHint = QFileInfo(target).suffix().isEmpty();
        const char *format = needsFormatHint ? "PNG" : NULL;
        const bool saved = getPixmap().save(target, format, 100);
        if (isAdaptiveIcon()) {
            qDebug().noquote() << "Adaptive icon save target:" << Path::display(target)
                               << (needsFormatHint ? "(PNG)" : "")
                               << (saved ? "OK" : "FAILED");
        } else if (!saved) {
            qWarning().noquote() << "Could not save icon:" << Path::display(target);
        }
        result = saved && result;
    }
    if (result && adaptiveDescriptor.needsXmlPatch()) {
        result = patchAdaptiveForeground();
    }
    return result;
}

bool Icon::replace(QPixmap pixmap)
{
    if (pixmap.isNull()) {
        return false;
    }
    if (isAdaptiveIcon()) {
        qDebug().noquote() << "Replacing adaptive icon:\n" + getToolTip();
        qDebug().noquote() << "Adaptive icon replacement policy:"
                           << "foreground-only;"
                           << "background layer is preserved;"
                           << (adaptiveDescriptor.usesCustomForeground()
                               ? "custom foreground XML patch will be used"
                               : "existing foreground bitmap will be overwritten");
    }
    setPixmap(pixmap);
    modified = true;
    emit updated();
    return true;
}

bool Icon::resize(QSize size)
{
    setPixmap(pixmap.scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    modified = true;
    emit updated();
    return !pixmap.isNull();
}

bool Icon::resize(int w, int h)
{
    return resize(QSize(w, h));
}

bool Icon::revert()
{
    angle      = 0;
    isColorize = false;
    isFlipX    = false;
    isFlipY    = false;
    color      = Qt::black;
    depth      = 1.0;
    blur       = 1.0;
    corners    = 0;
    setPixmap(virtualIcon ? originalPixmap : QPixmap(filePath));
    modified = false;
    emit updated();
    return !pixmap.isNull();
}

QString Icon::getFilename() const
{
    return filePath;
}

QString Icon::getAdaptiveXmlPath() const
{
    return adaptiveDescriptor.xmlPath;
}

const AdaptiveIconDescriptor &Icon::getAdaptiveDescriptor() const
{
    return adaptiveDescriptor;
}

Icon::Type Icon::getType() const
{
    return type;
}

Icon::Scope Icon::getScope() const
{
    return scope;
}

bool Icon::isAdaptiveIcon() const
{
    return adaptiveDescriptor.isValid();
}

QString Icon::getTitle() const
{
    if (type == TvBanner) {
        return tr("TV Banner");
    }
    return getQualifiers().join(" - ").toUpper();
}

QString Icon::getToolTip() const
{
    if (!isAdaptiveIcon()) {
        return tr("Bitmap icon") + "\n" + Path::display(filePath);
    }

    QStringList lines;
    lines << tr("Adaptive XML icon");
    lines << "";
    lines << tr("XML:") << Path::display(adaptiveDescriptor.xmlPath);
    if (!adaptiveDescriptor.previewSource.isEmpty()) {
        lines << "";
        lines << tr("Preview source:") << adaptiveDescriptor.previewSource;
        if (!adaptiveDescriptor.previewPath.isEmpty()) {
            lines << Path::display(adaptiveDescriptor.previewPath);
        }
    }
    lines << "";
    lines << tr("Foreground:") << adaptiveDescriptor.foregroundRef;
    if (!adaptiveDescriptor.foregroundPath.isEmpty()) {
        lines << Path::display(adaptiveDescriptor.foregroundPath);
    } else {
        lines << tr("Vector/XML foreground");
    }
    lines << "";
    lines << tr("Background:") << adaptiveDescriptor.backgroundRef;
    if (!adaptiveDescriptor.backgroundPath.isEmpty()) {
        lines << Path::display(adaptiveDescriptor.backgroundPath);
    } else if (adaptiveDescriptor.backgroundColor.isValid()) {
        lines << adaptiveDescriptor.backgroundColor.name(QColor::HexArgb).toUpper();
    } else {
        lines << tr("Not resolved");
    }
    if (!adaptiveDescriptor.monochromeRef.isEmpty() || adaptiveDescriptor.monochromeRenderable) {
        lines << "";
        lines << tr("Monochrome:") << (adaptiveDescriptor.monochromeRef.isEmpty() ? tr("inline layer") : adaptiveDescriptor.monochromeRef);
        if (!adaptiveDescriptor.monochromePath.isEmpty()) {
            lines << Path::display(adaptiveDescriptor.monochromePath);
        } else if (adaptiveDescriptor.monochromeColor.isValid()) {
            lines << adaptiveDescriptor.monochromeColor.name(QColor::HexArgb).toUpper();
        } else if (adaptiveDescriptor.monochromeRenderable) {
            lines << tr("Vector/XML monochrome layer");
        } else {
            lines << tr("Not resolved");
        }
        lines << tr("Used by Android themed icons; not used for the normal preview.");
    }
    lines << "";
    lines << tr("Write-back:");
    lines << tr("Mode: foreground-only replacement");
    if (!adaptiveDescriptor.foregroundPath.isEmpty()) {
        lines << tr("Target: existing bitmap foreground");
    } else {
        lines << tr("Target: custom bitmap foreground");
    }
    if (!adaptiveDescriptor.customForegroundRef.isEmpty()) {
        lines << adaptiveDescriptor.customForegroundRef;
    }
    if (!saveTargets.isEmpty()) {
        QStringList displayTargets;
        foreach (const QString &target, saveTargets) {
            displayTargets << Path::display(target);
        }
        lines << displayTargets.join("\n");
    }
    return lines.join("\n");
}

QPixmap Icon::getPixmap()
{
    return pixmapFx;
}

const QStringList &Icon::getQualifiers() const
{
    return qualifiers;
}

void Icon::setPixmap(const QPixmap &pixmap)
{
    this->pixmap = pixmap;
    this->pixmapFx = pixmap;
    applyEffects();
}

void Icon::setAngle(int value)
{
    angle = value;
    modified = true;
    applyEffects();
}

void Icon::setColorize(bool enable)
{
    isColorize = enable;
    modified = true;
    applyEffects();
}

void Icon::setFlipX(bool value)
{
    isFlipX = value;
    modified = true;
    applyEffects();
}

void Icon::setFlipY(bool value)
{
    isFlipY = value;
    modified = true;
    applyEffects();
}

void Icon::setColor(QColor value)
{
    color = value;
    modified = true;
    applyEffects();
}

void Icon::setDepth(qreal value)
{
    depth = value;
    modified = true;
    applyEffects();
}

void Icon::setBlur(qreal radius)
{
    blur = radius;
    modified = true;
    applyEffects();
}

void Icon::setCorners(qreal radius)
{
    corners = radius;
    modified = true;
    applyEffects();
}

void Icon::applyEffects()
{
    pixmapFx = pixmap;

    // Apply color overlay:

    if (isColorize && !qFuzzyIsNull(depth)) {
        QLabel canvas;
        QGraphicsColorizeEffect *effect = new QGraphicsColorizeEffect();
        effect->setColor(color);
        effect->setStrength(depth);
        canvas.setPixmap(pixmapFx);
        canvas.setGraphicsEffect(effect);
        pixmapFx = canvas.grab();
    }

    // Apply rounded corners:

    if (!qFuzzyIsNull(corners)) {
        QImage canvas(pixmapFx.size(), QImage::Format_ARGB32_Premultiplied);
        QPainter painter(&canvas);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.fillRect(pixmapFx.rect(), Qt::transparent);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QBrush(pixmapFx));
        painter.setRenderHint(QPainter::Antialiasing);
        painter.drawRoundedRect(pixmapFx.rect(), corners, corners);
        painter.end();
        pixmapFx = QPixmap::fromImage(canvas);
    }

    // Apply blur:

    if (blur > 1.0) {
        QLabel canvas;
        QGraphicsBlurEffect *effect = new QGraphicsBlurEffect();
        effect->setBlurRadius(blur);
        effect->setBlurHints(QGraphicsBlurEffect::QualityHint);
        canvas.setPixmap(pixmapFx);
        canvas.setGraphicsEffect(effect);
        pixmapFx = canvas.grab();
    }

    // Apply flipping and rotation:

    if (isFlipX) { pixmapFx = pixmapFx.transformed(QTransform().scale(-1, 1)); }
    if (isFlipY) { pixmapFx = pixmapFx.transformed(QTransform().scale(1, -1)); }
    if (angle) { pixmapFx = pixmapFx.transformed(QTransform().rotate(angle)); }
}

bool Icon::patchAdaptiveForeground() const
{
    QFile file(adaptiveDescriptor.xmlPath);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        qWarning().noquote() << "Error: Could not open adaptive icon XML:" << Path::display(adaptiveDescriptor.xmlPath);
        return false;
    }

    QTextStream in(&file);
    setUtf8Encoding(in);
    QDomDocument doc;
    if (!doc.setContent(in.readAll())) {
        qWarning().noquote() << "Error: Could not parse adaptive icon XML:" << Path::display(adaptiveDescriptor.xmlPath);
        return false;
    }
    file.close();

    QDomElement root = doc.documentElement();
    if (root.tagName() != "adaptive-icon") {
        return false;
    }

    QDomElement foreground = root.firstChildElement("foreground");
    if (foreground.isNull()) {
        foreground = doc.createElement("foreground");
        root.appendChild(foreground);
    }
    foreground.setAttribute("android:drawable", adaptiveDescriptor.customForegroundRef);
    qDebug().noquote() << "Adaptive icon XML foreground patched:" << Path::display(adaptiveDescriptor.xmlPath) << "->" << adaptiveDescriptor.customForegroundRef;

    if (!file.open(QFile::WriteOnly | QFile::Text | QFile::Truncate)) {
        qWarning().noquote() << "Error: Could not save adaptive icon XML:" << Path::display(adaptiveDescriptor.xmlPath);
        return false;
    }

    QTextStream out(&file);
    setUtf8Encoding(out);
    doc.save(out, 4);
    return true;
}
