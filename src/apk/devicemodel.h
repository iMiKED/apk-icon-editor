#ifndef DEVICEMODEL_H
#define DEVICEMODEL_H

#include "device.h"
#include <QAbstractListModel>
#include <QJsonObject>

class DeviceModel : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit DeviceModel(QObject *parent = 0);
    ~DeviceModel();

    void add(Device *device);

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const Q_DECL_OVERRIDE;
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const Q_DECL_OVERRIDE;
    int rowCount(const QModelIndex &parent = QModelIndex()) const Q_DECL_OVERRIDE;

private:
    bool loadSharedConfig();
    bool addJsonDevice(const QJsonObject &object);
    void addFallbackDevices();
    QIcon iconFromConfig(const QString &path) const;

    QList<Device *> devices;
};

#endif // DEVICEMODEL_H
