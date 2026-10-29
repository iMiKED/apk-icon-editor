#include "iconsproxy.h"
#include <QFont>

IconsProxy::IconsProxy(QObject *parent) : QAbstractProxyModel(parent) {}
IconsProxy::~IconsProxy() = default;

void IconsProxy::setSourceModel(QAbstractItemModel *model)
{
    if (sourceModel()) disconnect(sourceModel(), nullptr, this, nullptr);
    QAbstractProxyModel::setSourceModel(model);
    if (model) {
        connect(model, &QAbstractItemModel::modelReset, this, &IconsProxy::rebuild);
        connect(model, &QAbstractItemModel::layoutChanged, this, &IconsProxy::rebuild);
        connect(model, &QAbstractItemModel::rowsInserted, this, &IconsProxy::rebuild);
        connect(model, &QAbstractItemModel::rowsRemoved, this, &IconsProxy::rebuild);
        connect(model, &QAbstractItemModel::dataChanged, this, [this](const QModelIndex &first, const QModelIndex &last) {
            for (int row = first.row(); row <= last.row(); ++row) {
                const QModelIndex mapped = mapFromSource(sourceModel()->index(row, 0));
                if (mapped.isValid()) emit dataChanged(mapped, mapped);
            }
        });
        connect(model, &QObject::destroyed, this, [this]() { rebuild(); });
    }
    rebuild();
}

void IconsProxy::rebuild()
{
    beginResetModel();
    qDeleteAll(root.children);
    root.children.clear();
    leaves.clear();
    QHash<QString, Node *> families;
    QHash<Node *, Node *> adaptiveGroups;
    QHash<Node *, Node *> legacyGroups;
    if (sourceModel()) {
        for (int row = 0; row < sourceModel()->rowCount(); ++row) {
            const QModelIndex source = sourceModel()->index(row, 0);
            Icon *icon = static_cast<Icon *>(source.internalPointer());
            if (!icon || (!showActivities && icon->getScope() == Icon::ScopeActivity)) continue;
            Node *parent = &root;
            if (icon->isAdaptiveIcon() || icon->isLegacyIcon()) {
                const QString key = QString::number(icon->getEntryRole()) + ':' + icon->getResourceRef() + ':' + icon->getFamilyPath();
                Node *family = families.value(key);
                if (!family) {
                    family = new Node;
                    family->parent = &root;
                    family->title = icon->getEntryRoleTitle() + "\n" + icon->getResourceRef();
                    family->expanded = families.isEmpty();
                    root.children << family;
                    families.insert(key, family);
                }
                QHash<Node *, Node *> &groups = icon->isAdaptiveIcon() ? adaptiveGroups : legacyGroups;
                parent = groups.value(family);
                if (!parent) {
                    parent = new Node;
                    parent->parent = family;
                    parent->title = icon->isAdaptiveIcon() ? tr("Adaptive XML") : tr("Bitmap - Legacy");
                    parent->expanded = icon->isAdaptiveIcon();
                    family->children << parent;
                    groups.insert(family, parent);
                }
            }
            Node *leaf = new Node;
            leaf->parent = parent;
            leaf->source = source;
            parent->children << leaf;
            leaves.insert(icon, leaf);
        }
    }
    for (Node *family : families) {
        for (Node *group : family->children) {
            group->title += QString(" (%1)").arg(group->children.count());
        }
        if (!adaptiveGroups.contains(family)) {
            Node *legacy = legacyGroups.value(family);
            legacy->expanded = true;
            legacy->title += "\n" + tr("Adaptive preview unavailable");
        }
    }
    endResetModel();
}

IconsProxy::Node *IconsProxy::node(const QModelIndex &index) const
{
    return index.isValid() ? static_cast<Node *>(index.internalPointer()) : const_cast<Node *>(&root);
}

QModelIndex IconsProxy::nodeIndex(Node *item) const
{
    return item && item->parent ? createIndex(item->parent->children.indexOf(item), 0, item) : QModelIndex();
}

QModelIndex IconsProxy::mapToSource(const QModelIndex &index) const
{
    return index.isValid() && index.model() == this ? QModelIndex(node(index)->source) : QModelIndex();
}

QModelIndex IconsProxy::mapFromSource(const QModelIndex &index) const
{
    return index.isValid() && index.model() == sourceModel()
            ? nodeIndex(leaves.value(static_cast<Icon *>(index.internalPointer()))) : QModelIndex();
}

QModelIndex IconsProxy::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent)) return QModelIndex();
    return createIndex(row, column, node(parent)->children.at(row));
}

QModelIndex IconsProxy::parent(const QModelIndex &child) const
{
    return child.isValid() ? nodeIndex(node(child)->parent) : QModelIndex();
}

int IconsProxy::rowCount(const QModelIndex &parent) const
{
    return parent.column() > 0 ? 0 : node(parent)->children.count();
}

int IconsProxy::columnCount(const QModelIndex &) const { return 1; }

QVariant IconsProxy::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) return QVariant();
    const Node *item = node(index);
    if (role == DefaultExpandedRole) return item->expanded;
    if (item->source.isValid()) {
        Icon *icon = static_cast<Icon *>(item->source.internalPointer());
        if (role == Qt::DisplayRole && device) return device->getIconTitle(*icon);
        return item->source.data(role);
    }
    if (role == Qt::DisplayRole || role == Qt::ToolTipRole) return item->title;
    if (role == Qt::FontRole) {
        QFont font;
        font.setBold(true);
        return font;
    }
    return QVariant();
}

Qt::ItemFlags IconsProxy::flags(const QModelIndex &index) const
{
    if (!index.isValid()) return Qt::NoItemFlags;
    return node(index)->source.isValid() ? Qt::ItemIsEnabled | Qt::ItemIsSelectable : Qt::ItemIsEnabled;
}

QModelIndex IconsProxy::defaultIcon() const
{
    Node *item = const_cast<Node *>(&root);
    while (!item->children.isEmpty()) {
        if (item->children.first()->source.isValid() && item != &root) {
            item = item->children.last();
        } else {
            item = item->children.first();
        }
    }
    return item->source.isValid() ? nodeIndex(item) : QModelIndex();
}

void IconsProxy::setDevice(Device *value)
{
    device = value;
    for (Node *leaf : leaves) {
        const QModelIndex changed = nodeIndex(leaf);
        emit dataChanged(changed, changed);
    }
}

void IconsProxy::setShowActivities(bool show)
{
    showActivities = show;
    rebuild();
}
