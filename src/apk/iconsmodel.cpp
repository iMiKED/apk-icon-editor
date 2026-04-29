#include "iconsmodel.h"
#include "globals.h"
#include <QDebug>

IconsModel::~IconsModel()
{
    qDeleteAll(icons);
}

void IconsModel::add(const QString &filename, Icon::Type type, Icon::Scope scope, Icon::EntryRole entryRole)
{
    // Add:

    Icon roleProbe(QString(), type, scope, entryRole);
    qDebug().noquote() << QString("Added %1: %2").arg(roleProbe.getEntryRoleTitle(), Path::display(filename));
    beginInsertRows(QModelIndex(), icons.count(), icons.count());
        Icon *icon = new Icon(filename, type, scope, entryRole);
        icons.append(icon);
        connect(icon, &Icon::updated, [=]() {
            QModelIndex index = this->index(icons.indexOf(icon), 0);
            emit dataChanged(index, index);
        });
    endInsertRows();

    // Sort:

    sortIcons();
    emit dataChanged(index(0, 0), index(icons.count() - 1, 0));
}

void IconsModel::add(const QString &filename, const QPixmap &pixmap, const QStringList &saveTargets, Icon::Type type, Icon::Scope scope, Icon::EntryRole entryRole)
{
    Icon roleProbe(QString(), type, scope, entryRole);
    qDebug().noquote() << QString("Added adaptive %1: %2").arg(roleProbe.getEntryRoleTitle(), Path::display(filename));
    beginInsertRows(QModelIndex(), icons.count(), icons.count());
        Icon *icon = new Icon(filename, pixmap, saveTargets, type, scope, entryRole);
        icons.append(icon);
        connect(icon, &Icon::updated, [=]() {
            QModelIndex index = this->index(icons.indexOf(icon), 0);
            emit dataChanged(index, index);
        });
    endInsertRows();

    sortIcons();
    emit dataChanged(index(0, 0), index(icons.count() - 1, 0));
}

void IconsModel::add(const QString &filename, const QPixmap &pixmap, const QStringList &saveTargets, const AdaptiveIconDescriptor &adaptiveDescriptor, Icon::Type type, Icon::Scope scope, Icon::EntryRole entryRole)
{
    Icon roleProbe(QString(), type, scope, entryRole);
    qDebug().noquote() << QString("Added adaptive %1: %2").arg(roleProbe.getEntryRoleTitle(), Path::display(filename));
    beginInsertRows(QModelIndex(), icons.count(), icons.count());
        Icon *icon = new Icon(filename, pixmap, saveTargets, adaptiveDescriptor, type, scope, entryRole);
        icons.append(icon);
        connect(icon, &Icon::updated, [=]() {
            QModelIndex index = this->index(icons.indexOf(icon), 0);
            emit dataChanged(index, index);
        });
    endInsertRows();

    sortIcons();
    emit dataChanged(index(0, 0), index(icons.count() - 1, 0));
}

void IconsModel::sortIcons()
{
    std::sort(icons.begin(), icons.end(), [](const Icon *a, const Icon *b) -> bool {
        if (a->getEntryPriority() != b->getEntryPriority()) {
            return a->getEntryPriority() < b->getEntryPriority();
        }
        return a->getType() < b->getType();
    });
}

bool IconsModel::remove(Icon *icon)
{
    QMutableListIterator<Icon *> it(icons);
    while (it.hasNext()) {
        if (icon == it.next()) {
            int index = icons.indexOf(icon);
            if (index >= 0) {
                beginRemoveRows(QModelIndex(), index, index);
                    it.remove();
                    delete icon;
                endRemoveRows();
                return true;
            }
        }
    }
    return false;
}

void IconsModel::clone(Icon *source)
{
    if (source) {
        foreach (Icon *icon, icons) {
            if (icon->getType() != Icon::Unknown) {
                if ((source->getType() == Icon::TvBanner) == (icon->getType() == Icon::TvBanner)) {
                    icon->replace(source->getPixmap());
                }
            }
        }
    }
}

void IconsModel::updateAdaptiveFamily(Icon *source)
{
    if (!source || !source->isAdaptiveIcon()) {
        return;
    }

    const QString xmlPath = source->getAdaptiveXmlPath();
    const QPixmap sourcePixmap = source->getPixmap();
    foreach (Icon *icon, icons) {
        if (icon == source || icon->getAdaptiveXmlPath() != xmlPath || icon->getType() == Icon::Unknown) {
            continue;
        }
        icon->replace(sourcePixmap.scaled(icon->width(), icon->height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    }
}

void IconsModel::save()
{
    foreach (Icon *icon, icons) {
        icon->save();
    }
}

bool IconsModel::hasIcon(Icon::Type type, Icon::Scope scope) const
{
    QListIterator<Icon *> it(icons);
    while (it.hasNext()) {
        Icon *icon = it.next();
        if (icon->getScope() == scope && icon->getType() == type) {
            return true;
        }
    }
    return false;
}

Icon *IconsModel::first()
{
    return icons.first();
}

Icon *IconsModel::last()
{
    return icons.last();
}

Icon *IconsModel::getLargestIcon()
{
    for (int i = icons.size() - 1; i >= 0; --i) {
        Icon *icon = icons.at(i);
        if (icon->getScope() == Icon::ScopeApplication && icon->getType() != Icon::TvBanner) {
            return icon;
        }
    }
    return nullptr;
}

bool IconsModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    Q_UNUSED(role);
    if (index.isValid()) {
        Icon *icon = icons.at(index.row());
        icon->setPixmap(value.value<QPixmap>());
        emit dataChanged(index, index);
        return true;
    }
    return false;
}

QVariant IconsModel::data(const QModelIndex &index, int role) const
{
    if (index.isValid()) {
        Icon *icon = icons.at(index.row());
        if (role == Qt::DisplayRole) {
            return icon->getTitle();
        } else if (role == Qt::DecorationRole) {
            return icon->getPixmap();
        } else if (role == Qt::ToolTipRole) {
            return icon->getToolTip();
        }
    }
    return QVariant();
}

QModelIndex IconsModel::index(int row, int column, const QModelIndex &parent) const
{
    if (hasIndex(row, column, parent)) {
        return createIndex(row, column, icons.at(row));
    }
    return QModelIndex();
}

int IconsModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return icons.count();
}
