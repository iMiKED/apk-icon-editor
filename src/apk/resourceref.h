#ifndef RESOURCEREF_H
#define RESOURCEREF_H

#include <QString>

class ResourceRef
{
public:
    ResourceRef();
    explicit ResourceRef(const QString &value);

    bool isValid() const;
    bool isFramework() const;
    bool isAttribute() const;
    bool isRawId() const;

    QString original() const;
    QString packageName() const;
    QString type() const;
    QString name() const;

private:
    void parse(const QString &value);

    QString m_original;
    QString m_packageName;
    QString m_type;
    QString m_name;
    bool m_valid;
    bool m_framework;
    bool m_attribute;
    bool m_rawId;
};

#endif // RESOURCEREF_H
