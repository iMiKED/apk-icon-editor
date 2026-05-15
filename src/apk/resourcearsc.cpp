#include "resourcearsc.h"
#include <QFile>
#include <QHash>
#include <QList>
#include <QtEndian>

namespace {

const quint16 RES_STRING_POOL_TYPE = 0x0001;
const quint16 RES_TABLE_TYPE = 0x0002;
const quint16 RES_TABLE_PACKAGE_TYPE = 0x0200;
const quint16 RES_TABLE_TYPE_TYPE = 0x0201;
const quint8 TYPE_REFERENCE = 0x01;
const quint8 TYPE_STRING = 0x03;
const quint8 TYPE_INT_COLOR_ARGB8 = 0x1c;
const quint8 TYPE_INT_COLOR_RGB8 = 0x1d;
const quint8 TYPE_INT_COLOR_ARGB4 = 0x1e;
const quint8 TYPE_INT_COLOR_RGB4 = 0x1f;
const quint32 UTF8_FLAG = 0x00000100;
const quint32 FLAG_COMPLEX = 0x0001;
const quint32 NO_ENTRY = 0xffffffff;

bool hasBytes(const QByteArray &data, int offset, int length)
{
    if (offset < 0 || length < 0) {
        return false;
    }
    const qsizetype dataSize = data.size();
    return qsizetype(offset) <= dataSize && qsizetype(length) <= dataSize - qsizetype(offset);
}

quint16 u16(const QByteArray &data, int offset)
{
    if (!hasBytes(data, offset, 2)) {
        return 0;
    }
    return qFromLittleEndian<quint16>(reinterpret_cast<const uchar *>(data.constData() + offset));
}

quint32 u32(const QByteArray &data, int offset)
{
    if (!hasBytes(data, offset, 4)) {
        return 0;
    }
    return qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(data.constData() + offset));
}

bool validChunk(const QByteArray &data, int offset)
{
    if (!hasBytes(data, offset, 8)) {
        return false;
    }
    const quint16 headerSize = u16(data, offset + 2);
    const quint32 size = u32(data, offset + 4);
    return headerSize >= 8
            && size >= headerSize
            && size <= quint32(data.size() - offset);
}

int encodedLength8(const QByteArray &data, int *offset)
{
    if (!hasBytes(data, *offset, 1)) {
        return 0;
    }
    const uchar first = uchar(data.at((*offset)++));
    if ((first & 0x80) == 0) {
        return first;
    }
    if (!hasBytes(data, *offset, 1)) {
        return first & 0x7f;
    }
    const uchar second = uchar(data.at((*offset)++));
    return ((first & 0x7f) << 8) | second;
}

int encodedLength16(const QByteArray &data, int *offset)
{
    if (!hasBytes(data, *offset, 2)) {
        return 0;
    }
    const quint16 first = u16(data, *offset);
    *offset += 2;
    if ((first & 0x8000) == 0) {
        return first;
    }
    if (!hasBytes(data, *offset, 2)) {
        return first & 0x7fff;
    }
    const quint16 second = u16(data, *offset);
    *offset += 2;
    return ((first & 0x7fff) << 16) | second;
}

class StringPool
{
public:
    bool parse(const QByteArray &bytes, int offset)
    {
        data = &bytes;
        chunk = offset;
        if (!validChunk(bytes, offset) || u16(bytes, offset) != RES_STRING_POOL_TYPE) {
            return false;
        }
        const quint16 headerSize = u16(bytes, offset + 2);
        size = u32(bytes, offset + 4);
        count = u32(bytes, offset + 8);
        flags = u32(bytes, offset + 16);
        stringsStart = u32(bytes, offset + 20);
        offsetsStart = offset + headerSize;
        if (stringsStart > quint32(bytes.size() - offset) || offsetsStart < 0 || offsetsStart > bytes.size()) {
            return false;
        }
        return count <= quint32((bytes.size() - offsetsStart) / 4);
    }

    QString at(quint32 index) const
    {
        if (!data || index >= count) {
            return QString();
        }
        int pos = chunk + stringsStart + u32(*data, offsetsStart + int(index) * 4);
        if (!hasBytes(*data, pos, 1)) {
            return QString();
        }
        if ((flags & UTF8_FLAG) != 0) {
            encodedLength8(*data, &pos);
            const int byteLength = encodedLength8(*data, &pos);
            if (!hasBytes(*data, pos, byteLength)) {
                return QString();
            }
            return QString::fromUtf8(data->constData() + pos, byteLength);
        }

        const int charLength = encodedLength16(*data, &pos);
        if (charLength < 0 || !hasBytes(*data, pos, charLength * 2)) {
            return QString();
        }
        QString result;
        result.reserve(charLength);
        for (int i = 0; i < charLength; ++i) {
            result.append(QChar(u16(*data, pos + i * 2)));
        }
        return result;
    }

private:
    const QByteArray *data = nullptr;
    int chunk = 0;
    quint32 size = 0;
    quint32 count = 0;
    quint32 flags = 0;
    quint32 stringsStart = 0;
    int offsetsStart = 0;
};

struct TableValue {
    enum Kind {
        Reference,
        Color,
        File
    };

    Kind kind = Reference;
    quint32 sourceId = 0;
    quint32 targetId = 0;
    QColor color;
    QString filePath;
    QStringList qualifiers;
};

QString keyForId(quint32 id, const QHash<quint32, QString> &idToKey)
{
    return idToKey.value(id);
}

QString densityQualifier(quint16 density)
{
    switch (density) {
        case 120: return "ldpi";
        case 160: return "mdpi";
        case 240: return "hdpi";
        case 320: return "xhdpi";
        case 480: return "xxhdpi";
        case 640: return "xxxhdpi";
        case 0xffff: return "anydpi";
        case 0xfffe: return "nodpi";
        default: return QString();
    }
}

QString localePart(const QByteArray &data, int offset)
{
    if (!hasBytes(data, offset, 2)) {
        return QString();
    }
    const uchar first = uchar(data.at(offset));
    const uchar second = uchar(data.at(offset + 1));
    if (first == 0 || second == 0) {
        return QString();
    }
    if ((first & 0x80) == 0) {
        return QString(QChar(char(first))) + QChar(char(second));
    }
    return QString();
}

QStringList parseConfigQualifiers(const QByteArray &data, int offset)
{
    QStringList qualifiers;
    if (!hasBytes(data, offset, 4)) {
        return qualifiers;
    }

    const quint32 size = u32(data, offset);
    if (size == 0 || size > quint32(data.size() - offset)) {
        return qualifiers;
    }

    if (size >= 8) {
        const quint16 mcc = u16(data, offset + 4);
        const quint16 mnc = u16(data, offset + 6);
        if (mcc != 0) {
            qualifiers << QString("mcc%1").arg(mcc);
        }
        if (mnc != 0) {
            qualifiers << QString("mnc%1").arg(mnc);
        }
    }
    if (size >= 12) {
        const QString language = localePart(data, offset + 8);
        const QString country = localePart(data, offset + 10);
        if (!language.isEmpty()) {
            qualifiers << language;
            if (!country.isEmpty()) {
                qualifiers << ("r" + country.toUpper());
            }
        }
    }
    if (size >= 16) {
        const QString density = densityQualifier(u16(data, offset + 14));
        if (!density.isEmpty()) {
            qualifiers << density;
        }
    }
    if (size >= 28) {
        const quint16 sdk = u16(data, offset + 24);
        if (sdk != 0) {
            qualifiers << QString("v%1").arg(sdk);
        }
    }
    if (size >= 32) {
        const quint8 uiMode = quint8(data.at(offset + 29));
        if ((uiMode & 0x30) == 0x20) {
            qualifiers << "night";
        }
    }
    return qualifiers;
}

QColor colorForValue(quint8 dataType, quint32 value)
{
    if (dataType == TYPE_INT_COLOR_ARGB8) {
        return QColor::fromRgba(value);
    }
    if (dataType == TYPE_INT_COLOR_RGB8) {
        return QColor::fromRgba(0xff000000u | value);
    }
    if (dataType == TYPE_INT_COLOR_ARGB4 || dataType == TYPE_INT_COLOR_RGB4) {
        const int a4 = dataType == TYPE_INT_COLOR_ARGB4 ? int((value >> 12) & 0x0f) : 0x0f;
        const int r4 = int((value >> 8) & 0x0f);
        const int g4 = int((value >> 4) & 0x0f);
        const int b4 = int(value & 0x0f);
        return QColor((r4 << 4) | r4, (g4 << 4) | g4, (b4 << 4) | b4, (a4 << 4) | a4);
    }
    return QColor();
}

void parseTypeChunk(const QByteArray &data, int typeOffset, quint32 packageId,
                    const StringPool &typeStrings, const StringPool &keyStrings,
                    const StringPool &valueStrings,
                    QHash<quint32, QString> *idToKey, QList<TableValue> *values)
{
    if (!validChunk(data, typeOffset) || u16(data, typeOffset) != RES_TABLE_TYPE_TYPE) {
        return;
    }

    const quint8 typeId = quint8(data.at(typeOffset + 8));
    const quint32 entryCount = u32(data, typeOffset + 12);
    const quint32 entriesStart = u32(data, typeOffset + 16);
    const quint16 headerSize = u16(data, typeOffset + 2);
    const QStringList qualifiers = parseConfigQualifiers(data, typeOffset + 20);
    if (typeId == 0) {
        return;
    }
    const QString typeName = typeStrings.at(typeId - 1);
    if (typeName.isEmpty() || entryCount == 0 || entriesStart == 0) {
        return;
    }

    const int offsetsStart = typeOffset + headerSize;
    if (!hasBytes(data, offsetsStart, 0)) {
        return;
    }
    const int safeEntryCount = int(qMin(entryCount, quint32((data.size() - offsetsStart) / 4)));
    for (int i = 0; i < safeEntryCount; ++i) {
        const quint32 entryOffset = u32(data, offsetsStart + i * 4);
        if (entryOffset == NO_ENTRY) {
            continue;
        }

        const int entryPos = typeOffset + int(entriesStart) + int(entryOffset);
        if (!hasBytes(data, entryPos, 8)) {
            continue;
        }

        const quint16 entrySize = u16(data, entryPos);
        const quint16 flags = u16(data, entryPos + 2);
        const quint32 keyIndex = u32(data, entryPos + 4);
        const QString keyName = keyStrings.at(keyIndex);
        if (keyName.isEmpty()) {
            continue;
        }

        const quint32 resourceId = (packageId << 24) | (quint32(typeId) << 16) | quint32(i);
        idToKey->insert(resourceId, typeName + "/" + keyName);

        if ((flags & FLAG_COMPLEX) != 0) {
            continue;
        }

        const int valuePos = entryPos + entrySize;
        if (!hasBytes(data, valuePos, 8)) {
            continue;
        }
        const quint8 dataType = quint8(data.at(valuePos + 3));
        const quint32 valueData = u32(data, valuePos + 4);
        if (dataType == TYPE_REFERENCE && valueData != 0 && valueData != resourceId) {
            TableValue ref;
            ref.kind = TableValue::Reference;
            ref.sourceId = resourceId;
            ref.targetId = valueData;
            ref.qualifiers = qualifiers;
            values->append(ref);
        } else if (dataType >= TYPE_INT_COLOR_ARGB8 && dataType <= TYPE_INT_COLOR_RGB4) {
            TableValue color;
            color.kind = TableValue::Color;
            color.sourceId = resourceId;
            color.color = colorForValue(dataType, valueData);
            color.qualifiers = qualifiers;
            if (color.color.isValid()) {
                values->append(color);
            }
        } else if (dataType == TYPE_STRING && (typeName == "drawable" || typeName == "mipmap")) {
            const QString path = valueStrings.at(valueData);
            if (path.startsWith("res/")) {
                TableValue file;
                file.kind = TableValue::File;
                file.sourceId = resourceId;
                file.filePath = path;
                file.qualifiers = qualifiers;
                values->append(file);
            }
        }
    }
}

void parsePackage(const QByteArray &data, int packageOffset,
                  const StringPool &valueStrings,
                  QHash<quint32, QString> *idToKey, QList<TableValue> *values)
{
    if (!validChunk(data, packageOffset) || u16(data, packageOffset) != RES_TABLE_PACKAGE_TYPE) {
        return;
    }

    const quint32 packageSize = u32(data, packageOffset + 4);
    const quint32 packageId = u32(data, packageOffset + 8);
    const quint32 typeStringsOffset = u32(data, packageOffset + 268);
    const quint32 keyStringsOffset = u32(data, packageOffset + 276);
    if (packageSize > quint32(data.size() - packageOffset)) {
        return;
    }
    const int packageEnd = packageOffset + int(packageSize);

    StringPool typeStrings;
    StringPool keyStrings;
    if (!typeStrings.parse(data, packageOffset + int(typeStringsOffset)) ||
        !keyStrings.parse(data, packageOffset + int(keyStringsOffset))) {
        return;
    }

    int offset = packageOffset + u16(data, packageOffset + 2);
    while (offset <= packageEnd - 8 && hasBytes(data, offset, 8)) {
        if (!validChunk(data, offset)) {
            break;
        }
        const quint16 type = u16(data, offset);
        const quint32 size = u32(data, offset + 4);
        if (type == RES_TABLE_TYPE_TYPE) {
            parseTypeChunk(data, offset, packageId, typeStrings, keyStrings, valueStrings, idToKey, values);
        }
        offset += int(size);
    }
}

} // namespace

ResourceArsc::Table ResourceArsc::readTable(const QString &filePath)
{
    Table table;
    QFile file(filePath);
    if (!file.open(QFile::ReadOnly)) {
        return table;
    }

    const QByteArray data = file.readAll();
    if (!validChunk(data, 0) || u16(data, 0) != RES_TABLE_TYPE) {
        return table;
    }

    StringPool valueStrings;
    const int firstChunkOffset = u16(data, 0 + 2);
    if (!valueStrings.parse(data, firstChunkOffset)) {
        return table;
    }

    QHash<quint32, QString> idToKey;
    QList<TableValue> values;

    int offset = firstChunkOffset;
    while (hasBytes(data, offset, 8)) {
        if (!validChunk(data, offset)) {
            break;
        }
        const quint16 type = u16(data, offset);
        const quint32 size = u32(data, offset + 4);
        if (type == RES_TABLE_PACKAGE_TYPE) {
            parsePackage(data, offset, valueStrings, &idToKey, &values);
        }
        offset += int(size);
    }

    foreach (const TableValue &value, values) {
        const QString source = keyForId(value.sourceId, idToKey);
        if (source.isEmpty()) {
            continue;
        }

        if (value.kind == TableValue::Reference) {
            const QString target = keyForId(value.targetId, idToKey);
            if (!target.isEmpty()) {
                Alias alias;
                alias.key = source;
                alias.value = "@" + target;
                alias.qualifiers = value.qualifiers;
                table.aliases.append(alias);
            }
        } else if (value.kind == TableValue::Color) {
            Color color;
            color.key = source;
            color.color = value.color;
            color.qualifiers = value.qualifiers;
            table.colors.append(color);
        } else if (value.kind == TableValue::File) {
            File fileValue;
            fileValue.key = source;
            fileValue.path = value.filePath;
            fileValue.qualifiers = value.qualifiers;
            table.files.append(fileValue);
        }
    }
    return table;
}

QList<ResourceArsc::Alias> ResourceArsc::readReferenceAliases(const QString &filePath)
{
    return readTable(filePath).aliases;
}
