#include "titlenode.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QStringConverter>
#endif

String::String(const QString &filename, const QDomNode &node)
{
    this->filename = filename;
    this->node = node;
    this->document = node.ownerDocument();
    modified = false;
}

void String::setValue(const QString &value)
{
    node.firstChild().setNodeValue(value);
    modified = true;
}

QString String::getValue() const
{
    return node.firstChild().nodeValue();
}

QString String::getFilePath() const
{
    return filename;
}

bool String::wasModified() const
{
    return modified;
}

void String::save() const
{
    QFile xml(filename);
    if (xml.open(QFile::ReadWrite | QFile::Text)) {
        xml.resize(0);
        QTextStream stream(&xml);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        stream.setEncoding(QStringConverter::Utf8);
#else
        stream.setCodec("UTF-8");
#endif
        document.save(stream, 4);
    } else {
        qWarning() << "Error: Could not save titles resource file";
    }
}
