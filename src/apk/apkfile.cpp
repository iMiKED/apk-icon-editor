#include "apkfile.h"
#include "adaptiveicon.h"
#include "globals.h"
#include <QApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QEventLoop>
#include <QFileInfo>
#include <QTextStream>
#include <QDebug>

static void keepIconListUiResponsive()
{
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
}

static QString resourceKey(const QString &type, const QString &name)
{
    return type + "/" + name;
}

static QString resourceKey(const ResourceRef &ref)
{
    return ref.isValid() ? resourceKey(ref.type(), ref.name()) : QString();
}

static Icon::EntryRole entryRoleFor(const Manifest::IconEntry &entry)
{
    if (entry.alias && entry.round) {
        return Icon::EntryActivityAliasRoundIcon;
    }
    if (entry.alias) {
        return Icon::EntryActivityAliasIcon;
    }
    if (entry.round) {
        return Icon::EntryActivityRoundIcon;
    }
    return Icon::EntryActivityIcon;
}

static QString xmlRootTag(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        return QString();
    }

    QDomDocument doc;
    if (!doc.setContent(file.readAll())) {
        return QString();
    }
    return doc.documentElement().tagName();
}

static bool isRenderableStandaloneDrawableRoot(const QString &tag)
{
    return tag == "vector"
            || tag == "layer-list"
            || tag == "shape"
            || tag == "bitmap"
            || tag == "inset"
            || tag == "rotate"
            || tag == "item";
}

Apk::File::File(const QString &contentsPath, const QStringList &splitContentsPaths)
{
    // Be careful with the "contentsPath" variable: this directory is recursively removed in the destructor.

    this->contentsPath = QDir::cleanPath(QDir::fromNativeSeparators(contentsPath));
    foreach (const QString &path, splitContentsPaths) {
        const QString clean = QDir::cleanPath(QDir::fromNativeSeparators(path));
        if (!clean.isEmpty() && !this->splitContentsPaths.contains(clean)) {
            this->splitContentsPaths << clean;
        }
    }

    // Parse application manifest:

    manifest = new Manifest(QDir::cleanPath(this->contentsPath + "/AndroidManifest.xml"),
                            QDir::cleanPath(this->contentsPath + "/apktool.yml"));
    manifestModel.initialize(manifest);
    const QString appIconAttribute = manifest->getApplicationIcon();
    const QString appRoundIconAttribute = manifest->getApplicationRoundIcon();
    const QString appIconCategory = appIconAttribute.split('/').value(0).mid(1);
    const QString appIconFilename = appIconAttribute.split('/').value(1);
    const QString appBannerAttribute = manifest->getApplicationBanner();
    const QString appBannerCategory = appBannerAttribute.split('/').value(0).mid(1);
    const QString appBannerFilename = appBannerAttribute.split('/').value(1);
    const QList<Manifest::IconEntry> launcherIconEntries = manifest->getLauncherIconEntries();
    QStringList activityBanners = manifest->getActivityBanners();
    const QString appLabelAttribute = manifest->getApplicationLabel();
    const QString appLabelKey = appLabelAttribute.startsWith("@string/") ? appLabelAttribute.mid(QString("@string/").length()) : QString();

    // Parse resource directories:

    ResourceResolver resolver(this->contentsPath, this->splitContentsPaths);
    foreach (const Manifest::IconEntry &entry, launcherIconEntries) {
        addAdaptiveIcons(resolver, ResourceRef(entry.value), Icon::ScopeActivity, entryRoleFor(entry));
        keepIconListUiResponsive();
    }
    const bool appAdaptiveIcon = addAdaptiveIcons(resolver, ResourceRef(appIconAttribute), Icon::ScopeApplication, Icon::EntryApplicationIcon);
    keepIconListUiResponsive();
    if (!appAdaptiveIcon && !appRoundIconAttribute.isEmpty()) {
        addAdaptiveIcons(resolver, ResourceRef(appRoundIconAttribute), Icon::ScopeApplication, Icon::EntryApplicationRoundIcon);
        keepIconListUiResponsive();
    }

    const QStringList drawableExtensions = QStringList() << "png" << "jpg" << "jpeg" << "gif" << "webp";
    QDirIterator categories(QDir::cleanPath(this->contentsPath + "/res"), QDir::Dirs | QDir::NoDotAndDotDot);
    while (categories.hasNext()) {

        const QFileInfo category(categories.next());
        const QString categoryTitle = category.fileName().split('-').first(); // E.g., "drawable", "values"...

        QDirIterator resources(category.filePath(), QDir::Files);
        while (resources.hasNext()) {

            const QFileInfo resource(resources.next());

            if (drawableExtensions.contains(resource.suffix())) {
                const bool isAdaptiveLayer = isAdaptiveLayerResource(categoryTitle, resource.baseName());

                // Parse application icons:

                if (!appAdaptiveIcon && categoryTitle == appIconCategory) {
                    if (resource.baseName() == appIconFilename) {
                        if (isAdaptiveLayer) {
                            qDebug().noquote() << "Skipping adaptive layer fallback resource:" << Path::display(resource.filePath());
                            continue;
                        }
                        const QString icon = resource.filePath();
                        thumbnail.addFile(icon);
                        iconsModel.add(icon, Icon::Unknown, Icon::ScopeApplication, Icon::EntryApplicationIcon);
                    }
                }

                // Parse application banners:

                if (!appBannerFilename.isEmpty()) {
                    if (categoryTitle == appBannerCategory) {
                        if (resource.baseName() == appBannerFilename) {
                            iconsModel.add(resource.filePath(), Icon::TvBanner, Icon::ScopeApplication, Icon::EntryApplicationIcon);
                        }
                    }
                }

                // Parse activity icons:

                foreach (const Manifest::IconEntry &activityIconEntry, launcherIconEntries) {
                    const QString activityIconAttribute = activityIconEntry.value;
                    const QString activityIconCategory = activityIconAttribute.split('/').value(0).mid(1);
                    if (activityIconCategory == categoryTitle) {
                        const QString activityIconFilename = activityIconAttribute.split('/').value(1);
                        if (resource.baseName() == activityIconFilename) {
                            if (isAdaptiveLayer) {
                                qDebug().noquote() << "Skipping adaptive layer activity fallback resource:" << Path::display(resource.filePath());
                                continue;
                            }
                            iconsModel.add(resource.filePath(), Icon::Unknown, Icon::ScopeActivity, entryRoleFor(activityIconEntry));
                        }
                    }
                }

                // Parse activity banners:

                foreach (const QString &activityBannerAttribute, activityBanners) {
                    const QString activityBannerCategory = activityBannerAttribute.split('/').value(0).mid(1);
                    if (activityBannerCategory == categoryTitle) {
                        const QString activityBannerFilename = activityBannerAttribute.split('/').value(1);
                        if (resource.baseName() == activityBannerFilename) {
                            iconsModel.add(resource.filePath(), Icon::TvBanner, Icon::ScopeActivity);
                        }
                    }
                }
            }

            // Parse application titles:

            if (!appLabelKey.isEmpty()) {
                if (categoryTitle == "values") {
                    titlesModel.add(resource.filePath(), appLabelKey);
                }
            }
            keepIconListUiResponsive();
        }
    }
}

Apk::File::~File()
{
    delete manifest;
}

void Apk::File::saveIcons()
{
    iconsModel.save();
}

void Apk::File::saveTitles()
{
    titlesModel.save();
}

void Apk::File::removeFiles()
{
    QFileInfo fi(contentsPath);
    if (!contentsPath.isEmpty() && fi.exists() && !fi.isRoot()) {
        QDir(contentsPath).removeRecursively();
    }
}

bool Apk::File::addIcon(Icon::Type type)
{
    // TODO Mark APK as modified
    if (!iconsModel.hasIcon(type)) {
        if (type != Icon::TvBanner) {
            if (manifest->getApplicationIcon().isEmpty()) {
                manifest->addApplicationIcon();
            }
            QString filePath = getIconPath(type);
            if (!filePath.isEmpty()) {
                filePath.append(".png");
                QString directory = QFileInfo(filePath).path();
                QDir().mkdir(directory);
                Icon *icon = iconsModel.getLargestIcon();
                QPixmap pixmap = icon
                        ? icon->getPixmap()
                        : QPixmap(":/gfx/icon/256.png").scaled(
                              Device().getIconSize(type).size,
                              Qt::KeepAspectRatio,
                              Qt::SmoothTransformation
                        );
                if (pixmap.save(filePath)) {
                    iconsModel.add(filePath);
                    return true;
                }
            }
        } else {
            if (!manifest->addApplicationBanner()) {
                return false;
            }
            const QString filePath = contentsPath + "/res/drawable-xhdpi/banner_custom.png";
            QString directory = QFileInfo(filePath).path();
            QDir().mkdir(directory);
            QPixmap pixmap(":/gfx/blank/tv.png");
            if (pixmap.save(filePath)) {
                iconsModel.add(filePath, Icon::TvBanner);
                return true;
            }
        }
    }
    return false;
}

void Apk::File::removeIcon(Icon *icon)
{
    QFileInfo fi(icon->getFilename());
    QString filePath = fi.path() + "/" + fi.baseName();
    QStringList icons;
    icons << filePath + ".png" << filePath + ".jpg" << filePath + ".jpeg" << filePath + ".gif" << filePath + ".webp";
    foreach (const QString &icon, icons) {
        QFile::remove(icon);
    }
    iconsModel.remove(icon);
}

bool Apk::File::addAdaptiveIcons(const ResourceResolver &resolver, const ResourceRef &iconRef, Icon::Scope scope, Icon::EntryRole entryRole)
{
    if (!iconRef.isValid()) {
        return false;
    }

    const ResourceResolver::Value xml = resolver.resolveXml(iconRef);
    if (!xml.found) {
        return false;
    }

    const QString rootTag = xmlRootTag(xml.filePath);
    if (rootTag != "adaptive-icon") {
        bool xmlDrawableAdded = false;
        if (isRenderableStandaloneDrawableRoot(rootTag)) {
            xmlDrawableAdded = addXmlDrawableIcon(resolver, iconRef, xml, rootTag, scope, entryRole);
            if (xmlDrawableAdded) {
                return true;
            }
        }
    }

    bool added = false;
    if (rootTag == "adaptive-icon") {
        const QList<Icon::Type> types = QList<Icon::Type>()
                << Icon::Ldpi
                << Icon::Mdpi
                << Icon::Hdpi
                << Icon::Tvdpi
                << Icon::Xhdpi
                << Icon::Xxhdpi
                << Icon::Xxxhdpi;

        Device device;
        foreach (Icon::Type type, types) {
            const QSize size = device.getIconSize(type).size;
            const AdaptiveIcon::Result result = AdaptiveIcon::resolve(resolver, iconRef, type, size);
            if (!result.valid) {
                continue;
            }

            adaptiveLayerRefs.insert(resourceKey(iconRef));

            QStringList saveTargets;
            AdaptiveIconDescriptor descriptor = result.descriptor;
            applySplitResourcePolicy(&descriptor);
            QPixmap previewPixmap = result.pixmap;
            bool usesReadyBitmapPreview = false;
            const ResourceResolver::Value directBitmap = resolver.resolveBitmap(iconRef, type);
            if (directBitmap.found) {
                const QPixmap directPixmap(directBitmap.filePath);
                if (!directPixmap.isNull()) {
                    previewPixmap = directPixmap;
                    usesReadyBitmapPreview = true;
                    descriptor.previewSource = "ready bitmap with the same resource id";
                    descriptor.previewPath = directBitmap.filePath;
                    applySplitResourcePolicy(&descriptor);
                    if (isBaseResourcePath(directBitmap.filePath)) {
                        saveTargets.append(directBitmap.filePath);
                    } else if (isSplitResourcePath(directBitmap.filePath)) {
                        qDebug().noquote() << "Adaptive icon ready bitmap is from a read-only split resource:" << Path::display(directBitmap.filePath);
                    }
                    qDebug().noquote() << "Adaptive icon preview uses ready bitmap resource:" << Path::display(directBitmap.filePath);
                }
            }
            if (!usesReadyBitmapPreview) {
                descriptor.previewSource = "composed XML layers with AOSP-style layer inset";
            }
            if (!descriptor.foregroundPath.isEmpty()) {
                if (isBaseResourcePath(descriptor.foregroundPath)) {
                    saveTargets.append(descriptor.foregroundPath);
                } else if (isSplitResourcePath(descriptor.foregroundPath)) {
                    qDebug().noquote() << "Adaptive icon foreground is from a read-only split resource:" << Path::display(descriptor.foregroundPath);
                }
            } else {
                const QString qualifier = ResourceResolver::qualifierForType(type);
                const int resIndex = result.xmlPath.indexOf("/res/");
                if (!qualifier.isEmpty() && resIndex >= 0 && isBaseResourcePath(result.xmlPath)) {
                    const QString contentsRoot = result.xmlPath.left(resIndex);
                    const QString customName = iconRef.name() + "_foreground_custom";
                    const QString resourceType = iconRef.type().isEmpty() ? "mipmap" : iconRef.type();
                    saveTargets.append(QDir::cleanPath(QString("%1/res/%2-%3/%4.png").arg(contentsRoot, resourceType, qualifier, customName)));
                    descriptor.customForegroundRef = QString("@%1/%2").arg(resourceType, customName);
                } else if (isSplitResourcePath(result.xmlPath)) {
                    qDebug().noquote() << "Adaptive icon XML is from a read-only split resource:" << Path::display(result.xmlPath);
                }
            }
            applySplitResourcePolicy(&descriptor);
            const QString foregroundKey = resourceKey(ResourceRef(descriptor.foregroundRef));
            const QString backgroundKey = resourceKey(ResourceRef(descriptor.backgroundRef));
            const QString monochromeKey = resourceKey(ResourceRef(descriptor.monochromeRef));
            if (!foregroundKey.isEmpty()) {
                adaptiveLayerRefs.insert(foregroundKey);
            }
            if (!backgroundKey.isEmpty()) {
                adaptiveLayerRefs.insert(backgroundKey);
            }
            if (!monochromeKey.isEmpty()) {
                adaptiveLayerRefs.insert(monochromeKey);
            }
            saveTargets.removeDuplicates();
            if (scope == Icon::ScopeApplication) {
                thumbnail.addPixmap(previewPixmap);
            }
            iconsModel.add(result.xmlPath, previewPixmap, saveTargets, descriptor, type, scope, entryRole);
            keepIconListUiResponsive();
            added = true;
        }
    }

    if (!added) {
        QStringList addedFiles;
        const QList<ResourceResolver::Candidate> bitmapFallbacks = resolver.bitmapCandidatesByName(iconRef.name());
        foreach (const ResourceResolver::Candidate &fallback, bitmapFallbacks) {
            if (addedFiles.contains(fallback.filePath)) {
                continue;
            }
            if (isAdaptiveLayerResource(fallback.dirName.split('-').first(), QFileInfo(fallback.filePath).completeBaseName())) {
                qDebug().noquote() << "Skipping adaptive layer bitmap fallback:" << Path::display(fallback.filePath);
                continue;
            }

            if (scope == Icon::ScopeApplication) {
                thumbnail.addFile(fallback.filePath);
            }

            iconsModel.add(fallback.filePath, fallback.type, scope, entryRole);
            addedFiles.append(fallback.filePath);
            keepIconListUiResponsive();
            added = true;
        }
    }

    if (!added) {
        if (rootTag == "adaptive-icon") {
            qDebug() << "Adaptive icon was found but could not be rendered:" << iconRef.original();
        } else {
            qDebug() << "XML drawable icon was found but could not be rendered:" << iconRef.original() << "root tag:" << rootTag;
        }
    }
    return added;
}

bool Apk::File::addXmlDrawableIcon(const ResourceResolver &resolver, const ResourceRef &iconRef, const ResourceResolver::Value &xml, const QString &rootTag, Icon::Scope scope, Icon::EntryRole entryRole)
{
    bool added = false;
    const QList<Icon::Type> types = QList<Icon::Type>()
            << Icon::Ldpi
            << Icon::Mdpi
            << Icon::Hdpi
            << Icon::Tvdpi
            << Icon::Xhdpi
            << Icon::Xxhdpi
            << Icon::Xxxhdpi;

    Device device;
    foreach (Icon::Type type, types) {
        const QSize size = device.getIconSize(type).size;
        const QPixmap preview = AdaptiveIcon::renderDrawableXml(resolver, xml.filePath, type, size);
        if (preview.isNull()) {
            continue;
        }

        XmlDrawableIconDescriptor descriptor;
        descriptor.xmlPath = xml.filePath;
        descriptor.resourceRef = iconRef.original();
        descriptor.rootTag = rootTag;
        descriptor.previewSource = "rendered XML drawable";

        if (scope == Icon::ScopeApplication) {
            thumbnail.addPixmap(preview);
        }
        iconsModel.add(xml.filePath, preview, QStringList(), descriptor, type, scope, entryRole);
        keepIconListUiResponsive();
        added = true;
    }

    return added;
}

bool Apk::File::isAdaptiveLayerResource(const QString &resourceType, const QString &resourceName) const
{
    return adaptiveLayerRefs.contains(resourceKey(resourceType, resourceName));
}

bool Apk::File::isBaseResourcePath(const QString &path) const
{
    const QString clean = QDir::cleanPath(QDir::fromNativeSeparators(path));
    if (isSplitResourcePath(clean)) {
        return false;
    }
    return clean == contentsPath || clean.startsWith(contentsPath + "/");
}

bool Apk::File::isSplitResourcePath(const QString &path) const
{
    const QString clean = QDir::cleanPath(QDir::fromNativeSeparators(path));
    foreach (const QString &splitPath, splitContentsPaths) {
        if (clean == splitPath || clean.startsWith(splitPath + "/")) {
            return true;
        }
    }
    return false;
}

void Apk::File::applySplitResourcePolicy(AdaptiveIconDescriptor *descriptor) const
{
    if (!descriptor) {
        return;
    }

    descriptor->splitResourcesAvailable = !splitContentsPaths.isEmpty();
    QStringList paths;
    paths << descriptor->xmlPath
          << descriptor->foregroundPath
          << descriptor->backgroundPath
          << descriptor->monochromePath
          << descriptor->previewPath;
    paths.removeAll(QString());
    foreach (const QString &path, paths) {
        if (isSplitResourcePath(path)) {
            descriptor->usesReadOnlySplitResources = true;
            descriptor->splitResourcePaths << QDir::cleanPath(QDir::fromNativeSeparators(path));
        }
    }
    descriptor->splitResourcePaths.removeDuplicates();
}

// Getters:

QString Apk::File::getFilePath() const { return filePath; }
QString Apk::File::getContentsPath() const { return contentsPath; }
QIcon Apk::File::getThumbnail() const { return thumbnail; }

bool Apk::File::getApksigner() const { return isApksigner; }
bool Apk::File::getSmali() const { return isSmali; }
bool Apk::File::getSign() const { return isSign; }
bool Apk::File::getZipalign() const { return isZipalign; }
bool Apk::File::getKeystore() const { return isKeystore; }

QString Apk::File::getApktool() const { return apktool; }
QString Apk::File::getFilePem() const { return filePem; }
QString Apk::File::getFilePk8() const { return filePk8; }
QString Apk::File::getFileKeystore() const { return fileKeystore; }
QString Apk::File::getPassKeystore() const { return passKeystore; }
QString Apk::File::getAlias() const { return alias; }
QString Apk::File::getPassAlias() const { return passAlias; }

// Setters:

void Apk::File::setFilePath(QString filepath) { this->filePath = filepath; }
void Apk::File::setApksigner(bool value) { isApksigner = value; }
void Apk::File::setSmali(bool value) { isSmali = value; }
void Apk::File::setZipalign(bool value) { isZipalign = value; }
void Apk::File::setSign(bool value) { isSign = value; }
void Apk::File::setKeystore(bool value) { isKeystore = value; }

void Apk::File::setApktool(QString path) { apktool = path; }
void Apk::File::setFilePemPk8(QString pem, QString pk8) { filePem = pem; filePk8 = pk8; }
void Apk::File::setFileKeystore(QString filename, QString alias, QString passKeystore, QString passAlias)
{
    fileKeystore = filename;
    this->alias = alias;
    this->passKeystore = passKeystore;
    this->passAlias = passAlias;
}

QString Apk::File::getIconPath(Icon::Type type)
{
    const QString iconAttribute = manifest->getApplicationIcon();
    const QString iconCategory = iconAttribute.split('/').value(0).mid(1);
    const QString iconFilename = iconAttribute.split('/').value(1);

    QString qualifier;
    switch (type) {
        case Icon::Ldpi: qualifier = "ldpi"; break;
        case Icon::Mdpi: qualifier = "mdpi"; break;
        case Icon::Hdpi: qualifier = "hdpi"; break;
        case Icon::Tvdpi: qualifier = "tvdpi"; break;
        case Icon::Xhdpi: qualifier = "xhdpi"; break;
        case Icon::Xxhdpi: qualifier = "xxhdpi"; break;
        case Icon::Xxxhdpi: qualifier = "xxxhdpi"; break;
        case Icon::TvBanner: qualifier = "xhdpi"; break;
        default: return QString();
    }

    if (!iconFilename.isEmpty() && !qualifier.isEmpty()) {
        return QString("%1/res/%2-%3/%4").arg(contentsPath, iconCategory, qualifier, iconFilename);
    }
    return QString();
}
