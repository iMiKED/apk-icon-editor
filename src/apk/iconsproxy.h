#ifndef ICONSPROXY_H
#define ICONSPROXY_H

#include <QAbstractProxyModel>
#include <QPersistentModelIndex>
#include "device.h"

// Keep the editable source model flat; only the Icons view groups alternatives.
class IconsProxy : public QAbstractProxyModel
{
    Q_OBJECT
public:
    enum Role { DefaultExpandedRole = Qt::UserRole + 1 };
    explicit IconsProxy(QObject *parent = nullptr);
    ~IconsProxy();
    void setSourceModel(QAbstractItemModel *model) override;
    void setDevice(Device *device);
    void setShowActivities(bool show);
    QModelIndex defaultIcon() const;
    QModelIndex mapToSource(const QModelIndex &index) const override;
    QModelIndex mapFromSource(const QModelIndex &index) const override;
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

private:
    struct Node {
        Node *parent = nullptr;
        QList<Node *> children;
        QPersistentModelIndex source;
        QPersistentModelIndex previewSource;
        QString title;
        bool expanded = false;
        ~Node() { qDeleteAll(children); }
    };
    void rebuild();
    Node *node(const QModelIndex &index) const;
    QModelIndex nodeIndex(Node *node) const;
    Node root;
    QHash<Icon *, Node *> leaves;
    Device *device = nullptr;
    bool showActivities = false;
};

#endif
