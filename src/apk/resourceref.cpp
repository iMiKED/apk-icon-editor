#include "resourceref.h"

ResourceRef::ResourceRef()
    : m_valid(false),
      m_framework(false),
      m_attribute(false),
      m_rawId(false)
{
}

ResourceRef::ResourceRef(const QString &value)
    : ResourceRef()
{
    parse(value);
}

void ResourceRef::parse(const QString &value)
{
    m_original = value.trimmed();
    if (m_original.isEmpty()) {
        return;
    }

    m_attribute = m_original.startsWith('?');
    if (m_attribute) {
        return;
    }

    if (!m_original.startsWith('@')) {
        return;
    }

    QString ref = m_original.mid(1);
    m_rawId = ref.startsWith("0x", Qt::CaseInsensitive);
    if (m_rawId) {
        return;
    }

    const int colon = ref.indexOf(':');
    if (colon >= 0) {
        m_packageName = ref.left(colon);
        ref = ref.mid(colon + 1);
        m_framework = (m_packageName == "android");
    }

    const int slash = ref.indexOf('/');
    if (slash <= 0 || slash == ref.length() - 1) {
        return;
    }

    m_type = ref.left(slash);
    m_name = ref.mid(slash + 1);
    m_valid = !m_type.isEmpty() && !m_name.isEmpty() && !m_framework;
}

bool ResourceRef::isValid() const
{
    return m_valid;
}

bool ResourceRef::isFramework() const
{
    return m_framework;
}

bool ResourceRef::isAttribute() const
{
    return m_attribute;
}

bool ResourceRef::isRawId() const
{
    return m_rawId;
}

QString ResourceRef::original() const
{
    return m_original;
}

QString ResourceRef::packageName() const
{
    return m_packageName;
}

QString ResourceRef::type() const
{
    return m_type;
}

QString ResourceRef::name() const
{
    return m_name;
}
