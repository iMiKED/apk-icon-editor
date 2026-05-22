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
    const QString configPath = QDir::cleanPath(Path::Data::shared() + "devices.json");
    QFile input(configPath);
    if (!input.open(QFile::ReadOnly | QFile::Text)) {
        qWarning().noquote() << "Device presets config not found, using built-in fallback:" << Path::display(configPath);
        return false;
    }

    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(input.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isArray()) {
        qWarning().noquote() << "Device presets config is invalid, using built-in fallback:"
                             << Path::display(configPath) << error.errorString();
        return false;
    }

    const QJsonArray entries = doc.array();
    for (const QJsonValue &entry : entries) {
        if (entry.isObject() && !addJsonDevice(entry.toObject())) {
            qWarning().noquote() << "Skipping invalid device preset from" << Path::display(configPath);
        }
    }

    if (devices.isEmpty()) {
        qWarning().noquote() << "Device presets config has no valid entries, using built-in fallback:" << Path::display(configPath);
        return false;
    }

    qDebug().noquote() << QString("Loaded %1 device presets from %2").arg(devices.count()).arg(Path::display(configPath));
    return true;
}

bool DeviceModel::addJsonDevice(const QJsonObject &object)
{
    const QString title = object.value("title").toString().trimmed();
    const QJsonObject sizes = object.value("sizes").toObject();
    const QJsonObject banner = object.value("banner").toObject();
    const QStringList densityKeys = QStringList() << "ldpi" << "mdpi" << "hdpi" << "xhdpi" << "xxhdpi" << "xxxhdpi";
    QList<short> densitySizes;

    foreach (const QString &key, densityKeys) {
        const int size = sizes.value(key).toInt();
        if (size <= 0 || size > SHRT_MAX) {
            return false;
        }
        densitySizes << static_cast<short>(size);
    }

    const int bannerWidth = banner.value("width").toInt();
    const int bannerHeight = banner.value("height").toInt();
    if (title.isEmpty() || bannerWidth <= 0 || bannerHeight <= 0) {
        return false;
    }

    Device *device = new Device(title,
                                iconFromConfig(object.value("icon").toString()),
                                densitySizes.at(0),
                                densitySizes.at(1),
                                densitySizes.at(2),
                                densitySizes.at(3),
                                densitySizes.at(4),
                                densitySizes.at(5),
                                QSize(bannerWidth, bannerHeight));

    const QJsonObject hints = object.value("hints").toObject();
    const QMap<QString, Icon::Type> hintTypes = {
        { "ldpi", Icon::Ldpi },
        { "mdpi", Icon::Mdpi },
        { "hdpi", Icon::Hdpi },
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
        qDebug().noquote() << "Device preset icon uses built-in fallback:" << resourcePath;
        return QIcon(resourcePath);
    }

    qWarning().noquote() << "Device preset icon not found:" << Path::display(sharedPath);
    return QIcon();
}

void DeviceModel::addFallbackDevices()
{
    const QIcon iconAndroid(":/gfx/devices/android.png");
    const QIcon iconBlackberry(":/gfx/devices/blackberry.png");
    const QIcon iconAmazon(":/gfx/devices/amazon.png");

    const QSize tvBannerSize(320, 180);

    Device *android = new Device("Android Default", iconAndroid, 36, 48, 72, 96, 144, 192, tvBannerSize);
    Device *androidTv = new Device("Android TV / Leanback", iconAndroid, 36, 48, 72, 96, 144, 192, tvBannerSize);
    Device *bb_q10 = new Device("BlackBerry Q10", iconBlackberry, 90, 90, 90, 90, 90, 90, tvBannerSize); // Q10, Q5, Q10
    Device *bb_z10 = new Device("BlackBerry Z10", iconBlackberry, 110, 110, 110, 110, 110, 110, tvBannerSize);
    Device *bb_z30 = new Device("BlackBerry Z30", iconBlackberry, 96, 96, 96, 96, 96, 96, tvBannerSize); // Z30, Z3, Z30
    Device *bb_passport = new Device("BlackBerry Passport", iconBlackberry, 144, 144, 144, 144, 144, 144, tvBannerSize);
    Device *kindle1 = new Device("Kindle Fire (1st Gen)", iconAmazon, 36, 322, 72, 96, 144, 192, tvBannerSize);
    Device *kindle2 = new Device("Kindle Fire (2nd Gen)", iconAmazon, 36, 365, 72, 96, 144, 192, tvBannerSize);
    Device *kindle3 = new Device("Kindle Fire HD 7\" (2nd Gen)", iconAmazon, 36, 48, 425, 96, 144, 192, tvBannerSize);
    Device *kindle4 = new Device("Kindle Fire HD 8.9\" (2nd Gen)", iconAmazon, 36, 48, 675, 96, 144, 192, tvBannerSize);
    Device *kindle5 = new Device("Kindle Fire HD 7\" (3rd Gen)", iconAmazon, 36, 48, 375, 96, 144, 192, tvBannerSize);
    Device *kindle6 = new Device("Kindle Fire HDX 7\" (3rd Gen)", iconAmazon, 36, 48, 72, 562, 144, 192, tvBannerSize);
    Device *kindle7 = new Device("Kindle Fire HDX 8.9\" (3rd Gen)", iconAmazon, 36, 48, 72, 624, 144, 192, tvBannerSize);
    Device *kindle8 = new Device("Kindle Fire - All Models", iconAmazon, 36, 365, 675, 624, 144, 192, tvBannerSize);
    kindle1->setHint(Icon::Mdpi, "Carousel");
    kindle2->setHint(Icon::Mdpi, "Carousel");
    kindle3->setHint(Icon::Hdpi, "Carousel");
    kindle4->setHint(Icon::Hdpi, "Carousel");
    kindle5->setHint(Icon::Hdpi, "Carousel");
    kindle6->setHint(Icon::Xhdpi, "Carousel");
    kindle7->setHint(Icon::Xhdpi, "Carousel");
    kindle8->setHint(Icon::Mdpi, "Kindle Fire Carousel");
    kindle8->setHint(Icon::Hdpi, "Kindle Fire HD Carousel");
    kindle8->setHint(Icon::Xhdpi, "Kindle Fire HDX Carousel");

    add(android);
    add(androidTv);
    add(bb_q10);
    add(bb_z10);
    add(bb_z30);
    add(bb_passport);
    add(kindle1);
    add(kindle2);
    add(kindle3);
    add(kindle4);
    add(kindle5);
    add(kindle6);
    add(kindle7);
    add(kindle8);
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
