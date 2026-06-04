#include "devicemodel.h"
#include "globals.h"
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <climits>

DeviceModel::DeviceModel(QObject *parent) : QAbstractListModel(parent)
{
    if (!loadSharedConfig()) {
        addFallbackDevices();
    }
}

DeviceModel::~DeviceModel()
{
    qDeleteAll(devices);
}

bool DeviceModel::loadSharedConfig()
{
    const QString configPath = QDir::cleanPath(Path::Data::shared() + "icon-size-presets.json");
    QFile input(configPath);
    if (!input.open(QFile::ReadOnly | QFile::Text)) {
        qWarning().noquote() << "Icon size presets config not found, using built-in fallback:" << Path::display(configPath);
        return false;
    }

    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(input.readAll(), &error);
    if (error.error != QJsonParseError::NoError || (!doc.isArray() && !doc.isObject())) {
        qWarning().noquote() << "Icon size presets config is invalid, using built-in fallback:"
                             << Path::display(configPath) << error.errorString();
        return false;
    }

    QJsonArray entries;
    int legacyCount = 0;
    if (doc.isArray()) {
        entries = doc.array();
    } else {
        const QJsonObject root = doc.object();
        entries = root.value("presets").toArray();
        legacyCount = root.value("legacy").toArray().count();
    }
    for (const QJsonValue &entry : entries) {
        if (entry.isObject() && !addJsonDevice(entry.toObject())) {
            qWarning().noquote() << "Skipping invalid icon size preset from" << Path::display(configPath);
        }
    }

    if (devices.isEmpty()) {
        qWarning().noquote() << "Icon size presets config has no valid active entries, using built-in fallback:" << Path::display(configPath);
        return false;
    }

    qDebug().noquote() << QString("Loaded %1 icon size presets from %2; %3 legacy presets kept in config")
                          .arg(devices.count())
                          .arg(Path::display(configPath))
                          .arg(legacyCount);
    return true;
}

bool DeviceModel::addJsonDevice(const QJsonObject &object)
{
    const QString title = object.value("title").toString().trimmed();
    const QJsonObject sizes = object.value("sizes").toObject();
    const QStringList densityKeys = QStringList() << "ldpi" << "mdpi" << "hdpi" << "tvdpi" << "xhdpi" << "xxhdpi" << "xxxhdpi";
    QList<short> densitySizes;

    foreach (const QString &key, densityKeys) {
        const int size = sizes.value(key).toInt();
        if (size <= 0 || size > SHRT_MAX) {
            return false;
        }
        densitySizes << static_cast<short>(size);
    }

    if (title.isEmpty()) {
        return false;
    }

    const QJsonObject defaultBanner = object.value("banner").toObject();
    const QSize defaultBannerSize(defaultBanner.value("width").toInt(), defaultBanner.value("height").toInt());
    const QSize bannerFallback = defaultBannerSize.isValid() ? defaultBannerSize : QSize(320, 180);
    Device *device = new Device(title,
                                iconFromConfig(object.value("icon").toString()),
                                densitySizes.at(0),
                                densitySizes.at(1),
                                densitySizes.at(2),
                                densitySizes.at(4),
                                densitySizes.at(5),
                                densitySizes.at(6),
                                bannerFallback);
    device->setIconSize(Icon::Tvdpi, QSize(densitySizes.at(3), densitySizes.at(3)));

    bool hasBannerSize = defaultBannerSize.isValid();
    const QJsonObject banners = object.value("banners").toObject();
    foreach (const QString &key, densityKeys) {
        const QJsonObject banner = banners.value(key).toObject();
        const QSize size(banner.value("width").toInt(), banner.value("height").toInt());
        if (size.isValid()) {
            device->setTvBannerSize(key, size);
            hasBannerSize = true;
        }
    }
    if (!hasBannerSize) {
        delete device;
        return false;
    }

    const QJsonObject hints = object.value("hints").toObject();
    const QMap<QString, Icon::Type> hintTypes = {
        { "ldpi", Icon::Ldpi },
        { "mdpi", Icon::Mdpi },
        { "hdpi", Icon::Hdpi },
        { "tvdpi", Icon::Tvdpi },
        { "xhdpi", Icon::Xhdpi },
        { "xxhdpi", Icon::Xxhdpi },
        { "xxxhdpi", Icon::Xxxhdpi },
        { "tvBanner", Icon::TvBanner }
    };
    for (auto it = hintTypes.constBegin(); it != hintTypes.constEnd(); ++it) {
        const QString hint = hints.value(it.key()).toString().trimmed();
        if (!hint.isEmpty()) {
            device->setHint(it.value(), hint);
        }
    }

    add(device);
    return true;
}

QIcon DeviceModel::iconFromConfig(const QString &path) const
{
    const QString clean = QDir::cleanPath(QDir::fromNativeSeparators(path.trimmed()));
    if (clean.isEmpty()) {
        return QIcon();
    }

    if (clean.startsWith(":/")) {
        return QIcon(clean);
    }

    const QString sharedPath = QDir::cleanPath(Path::Data::shared() + clean);
    if (QFileInfo::exists(sharedPath)) {
        return QIcon(sharedPath);
    }

    const QString resourcePath = ":/" + clean;
    if (QFile::exists(resourcePath)) {
        qDebug().noquote() << "Icon size preset icon uses built-in fallback:" << resourcePath;
        return QIcon(resourcePath);
    }

    qWarning().noquote() << "Icon size preset icon not found:" << Path::display(sharedPath);
    return QIcon();
}

void DeviceModel::addFallbackDevices()
{
    const QIcon iconAndroid(":/gfx/devices/android.png");
    Device *android = new Device("Android Default", iconAndroid, 36, 48, 72, 96, 144, 192);
    Device *androidTv = new Device("Android TV / Google TV", iconAndroid, 80, 80, 120, 160, 240, 320);
    androidTv->setIconSize(Icon::Tvdpi, QSize(107, 107));

    QList<Device *> androidPresets = QList<Device *>() << android << androidTv;
    foreach (Device *device, androidPresets) {
        device->setTvBannerSize("mdpi", QSize(160, 90));
        device->setTvBannerSize("tvdpi", QSize(213, 120));
        device->setTvBannerSize("hdpi", QSize(240, 135));
        device->setTvBannerSize("xhdpi", QSize(320, 180));
        device->setTvBannerSize("xxhdpi", QSize(480, 270));
        device->setTvBannerSize("xxxhdpi", QSize(640, 360));
    }

    add(android);
    add(androidTv);
}

void DeviceModel::add(Device *device)
{
    beginInsertRows(QModelIndex(), devices.count(), devices.count());
    devices.append(device);
    endInsertRows();
}

QVariant DeviceModel::data(const QModelIndex &index, int role) const
{
    if (index.isValid()) {
        Device *device = devices.at(index.row());
        if (role == Qt::DisplayRole) {
            return device->getTitle();
        } else if (role == Qt::DecorationRole) {
            return device->getThumbnail();
        }
    }
    return QVariant();
}

QModelIndex DeviceModel::index(int row, int column, const QModelIndex &parent) const
{
    if (hasIndex(row, column, parent)) {
        return createIndex(row, column, devices.at(row));
    }
    return QModelIndex();
}

int DeviceModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return devices.count();
}
