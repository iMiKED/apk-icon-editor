#include "manifest.h"
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

Manifest::Manifest(const QString &xmlPath, const QString &ymlPath)
{
    this->xmlPath = xmlPath;
    this->ymlPath = ymlPath;

    // XML:

    QFile xmlFile(xmlPath);
    if (xmlFile.open(QFile::ReadOnly | QFile::Text)) {
        QTextStream stream(&xmlFile);
        setUtf8Encoding(stream);
        xml.setContent(stream.readAll());

        // Parse <application> node:

        applicationLabel = getXmlAttribute(QStringList() << "application" << "android:label");
        applicationIcon = getXmlAttribute(QStringList() << "application" << "android:icon");
        applicationRoundIcon = getXmlAttribute(QStringList() << "application" << "android:roundIcon");
        applicationBanner = getXmlAttribute(QStringList() << "application" << "android:banner");

        // Parse <activity> nodes:

        QDomNodeList nodes = xml.firstChildElement("manifest").firstChildElement("application").childNodes();
        for (int i = 0; i < nodes.count(); ++i) {
            QDomElement node = nodes.at(i).toElement();
            if (node.isElement() && node.nodeName() == "activity") {
                QDomElement activity = node.toElement();
                const bool enabled = activity.attribute("android:enabled", "true") != "false";
                const bool launcher = !findIntentByCategory(activity, "LAUNCHER").isNull();
                if (enabled && launcher) {
                    QDomAttr icon = activity.attributeNode("android:icon");
                    if (!icon.isNull()) {
                        activityIcons.append(icon);
                    }
                    QDomAttr roundIcon = activity.attributeNode("android:roundIcon");
                    if (!roundIcon.isNull()) {
                        activityIcons.append(roundIcon);
                    }
                    QDomAttr banner = activity.attributeNode("android:banner");
                    if (!banner.isNull()) {
                        activityBanners.append(banner);
                    }
                }
            }
            if (node.isElement() && node.nodeName() == "activity-alias") {
                QDomElement activity = node.toElement();
                const bool enabled = activity.attribute("android:enabled", "true") != "false";
                const bool launcher = !findIntentByCategory(activity, "LAUNCHER").isNull();
                if (enabled && launcher) {
                    QDomAttr icon = activity.attributeNode("android:icon");
                    if (!icon.isNull()) {
                        activityIcons.append(icon);
                    }
                    QDomAttr roundIcon = activity.attributeNode("android:roundIcon");
                    if (!roundIcon.isNull()) {
                        activityIcons.append(roundIcon);
                    }
                    QDomAttr banner = activity.attributeNode("android:banner");
                    if (!banner.isNull()) {
                        activityBanners.append(banner);
                    }
                }
            }
        }

#ifdef QT_DEBUG
        qDebug() << "Parsed app label:   " << applicationLabel.value();
        qDebug() << "Parsed app icon:    " << applicationIcon.value();
#endif
    }

    // YAML:

    regexMinSdk.setPatternOptions(QRegularExpression::MultilineOption);
    regexTargetSdk.setPatternOptions(QRegularExpression::MultilineOption);
    regexVersionCode.setPatternOptions(QRegularExpression::MultilineOption);
    regexVersionName.setPatternOptions(QRegularExpression::MultilineOption);
    regexMinSdk.setPattern("(^\\s*minSdkVersion:\\s*'?)\\d+('?$)");
    regexTargetSdk.setPattern("(^\\s*targetSdkVersion:\\s*'?)\\d+('?$)");
    regexVersionCode.setPattern("(^\\s*versionCode:\\s*'?)\\d+('?$)");
    regexVersionName.setPattern("(?<=^  versionName: ).+(?=$)");

    QFile ymlFile(ymlPath);
    if (ymlFile.open(QFile::ReadOnly | QFile::Text)) {
        QTextStream stream(&ymlFile);
        setUtf8Encoding(stream);
        yml = stream.readAll();
        minSdk = regexMinSdk.match(yml).captured(0).split(':').last().remove('\'').trimmed().toInt();
        targetSdk = regexTargetSdk.match(yml).captured(0).split(':').last().remove('\'').trimmed().toInt();
        versionCode = regexVersionCode.match(yml).captured(0).split(':').last().remove('\'').trimmed().toInt();
        versionName = regexVersionName.match(yml).captured();
#ifdef QT_DEBUG
        qDebug() << "Parsed minumum SDK: " << minSdk;
        qDebug() << "Parsed target SDK:  " << targetSdk;
        qDebug() << "Parsed version code:" << versionCode;
        qDebug() << "Parsed version name:" << versionName;
#endif
    }
}

Manifest::~Manifest()
{
}

QDomAttr Manifest::getXmlAttribute(QStringList &tree) const
{
    if (!tree.isEmpty()) {
        const QString attribute = tree.takeLast();
        QDomElement node = xml.firstChildElement("manifest");
        foreach (const QString &element, tree) {
            node = node.firstChildElement(element);
        }
        return node.attributeNode(attribute);
    }
    return QDomAttr();
}

QString Manifest::getApplicationIcon() const
{
    return applicationIcon.value();
}

QString Manifest::getApplicationRoundIcon() const
{
    return applicationRoundIcon.value();
}

QString Manifest::getApplicationBanner() const
{
    return applicationBanner.value();
}

QString Manifest::getApplicationLabel() const
{
    return applicationLabel.value();
}

QStringList Manifest::getActivityIcons() const
{
    QStringList values;
    for (int i = 0; i < activityIcons.count(); ++i) {
        const QString value = activityIcons.at(i).value();
        if (value != getApplicationIcon()) {
            values.append(value);
        }
    }
    values.removeDuplicates();
    return values;
}

QStringList Manifest::getActivityBanners() const
{
    QStringList values;
    for (int i = 0; i < activityBanners.count(); ++i) {
        const QString value = activityBanners.at(i).value();
        if (value != getApplicationBanner()) {
            values.append(value);
        }
    }
    values.removeDuplicates();
    return values;
}

int Manifest::getMinSdk() const
{
    return minSdk;
}

int Manifest::getTargetSdk() const
{
    return targetSdk;
}

int Manifest::getVersionCode() const
{
    return versionCode;
}

QString Manifest::getVersionName() const
{
    return versionName;
}

void Manifest::setApplicationIcon(const QString &value)
{
    applicationIcon.setValue(value);
    saveXml();
}

void Manifest::setApplicationLabel(const QString &value)
{
    applicationLabel.setValue(value);
    saveXml();
}

void Manifest::setMinSdk(int value)
{
    value = qMax(0, value);
    minSdk = value;
    yml.replace(regexMinSdk, QString("\\1%1\\2").arg(value));
    saveYml();
}

void Manifest::setTargetSdk(int value)
{
    value = qMax(1, value);
    targetSdk = value;
    yml.replace(regexTargetSdk, QString("\\1%1\\2").arg(value));
    saveYml();
}

void Manifest::setVersionCode(int value)
{
    value = qMax(0, value);
    versionCode = value;
    yml.replace(regexVersionCode, QString("\\1%1\\2").arg(value));
    saveYml();
}

void Manifest::setVersionName(const QString &value)
{
    versionName = value;
    yml.replace(regexVersionName, value);
    saveYml();
}

bool Manifest::addApplicationIcon()
{
    if (!applicationIcon.value().isEmpty()) {
        return false;
    }
    QDomElement application = xml.firstChildElement("manifest").firstChildElement("application");
    application.setAttribute("android:icon", "@drawable/icon_custom");
    applicationIcon = application.attributeNode("android:icon");
    return saveXml();
}

bool Manifest::addApplicationBanner()
{
    // TODO Refactor:

    QDomElement application = xml.firstChildElement("manifest").firstChildElement("application");
    QDomElement intent;

    QDomElement activity = application.firstChildElement("activity");
    while (!activity.isNull()) {
        intent = findIntentByCategory(activity, "LAUNCHER");
        if (!intent.isNull()) {
            break;
        }
        activity = activity.nextSiblingElement();
    }

    // If the "activity" tag was not found, try the "activity-alias" tag:

    if (intent.isNull()) {
        QDomElement activity = application.firstChildElement("activity-alias");
        while (!activity.isNull()) {
            intent = findIntentByCategory(activity, "LAUNCHER");
            if (!intent.isNull()) {
                break;
            }
            activity = activity.nextSiblingElement();
        }
    }

    if (!intent.isNull()) {
        // Add intent:
        QDomElement tvlauncher = xml.createElement("category");
        tvlauncher.setTagName("category");
        tvlauncher.setAttribute("android:name", "android.intent.category.LEANBACK_LAUNCHER");
        intent.appendChild(tvlauncher);
        // Add application attribute:
        application.setAttribute("android:banner", "@drawable/banner_custom");
        applicationBanner = getXmlAttribute(QStringList() << "application" << "android:banner");
        return saveXml();
    }
    return true;
}

bool Manifest::saveXml()
{
    QFile xmlFile(xmlPath);
    if (xmlFile.open(QFile::WriteOnly | QFile::Text | QFile::Truncate)) {
        QTextStream stream(&xmlFile);
        setUtf8Encoding(stream);
        xml.save(stream, 4);
        return true;
    } else {
        qWarning() << "Error: Could not save AndroidManifest.xml";
        return false;
    }
}

bool Manifest::saveYml()
{
    QFile ymlFile(ymlPath);
    if (ymlFile.open(QFile::WriteOnly | QFile::Text | QFile::Truncate)) {
        QTextStream stream(&ymlFile);
        setUtf8Encoding(stream);
        stream << yml;
        return true;
    } else {
        qWarning() << "Error: Could not save apktool.yml";
        return false;
    }
}

QDomElement Manifest::findIntentByCategory(QDomElement activity, QString category)
{
    QDomElement intent = activity.firstChildElement("intent-filter");
    while (!intent.isNull()) {
        QDomElement cat = intent.firstChildElement("category");
        while (!cat.isNull()) {
            if (cat.attribute("android:name") == "android.intent.category." + category) {
                return intent;
            }
            cat = cat.nextSiblingElement();
        }
        intent = intent.nextSiblingElement();
    }
    return QDomElement();
}
