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

struct RefValue {
    quint32 sourceId = 0;
    quint32 targetId = 0;
};

QString keyForId(quint32 id, const QHash<quint32, QString> &idToKey)
{
    return idToKey.value(id);
}

void parseTypeChunk(const QByteArray &data, int typeOffset, quint32 packageId,
                    const StringPool &typeStrings, const StringPool &keyStrings,
                    QHash<quint32, QString> *idToKey, QList<RefValue> *references)
{
    if (!validChunk(data, typeOffset) || u16(data, typeOffset) != RES_TABLE_TYPE_TYPE) {
        return;
    }

    const quint8 typeId = quint8(data.at(typeOffset + 8));
    const quint32 entryCount = u32(data, typeOffset + 12);
    const quint32 entriesStart = u32(data, typeOffset + 16);
    const quint16 headerSize = u16(data, typeOffset + 2);
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
            RefValue ref;
            ref.sourceId = resourceId;
            ref.targetId = valueData;
            references->append(ref);
        }
    }
}

void parsePackage(const QByteArray &data, int packageOffset,
                  QHash<quint32, QString> *idToKey, QList<RefValue> *references)
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
            parseTypeChunk(data, offset, packageId, typeStrings, keyStrings, idToKey, references);
        }
        offset += int(size);
    }
}

} // namespace

QMap<QString, QString> ResourceArsc::readReferenceAliases(const QString &filePath)
{
    QMap<QString, QString> aliases;
    QFile file(filePath);
    if (!file.open(QFile::ReadOnly)) {
        return aliases;
    }

    const QByteArray data = file.readAll();
    if (!validChunk(data, 0) || u16(data, 0) != RES_TABLE_TYPE) {
        return aliases;
    }

    QHash<quint32, QString> idToKey;
    QList<RefValue> references;

    int offset = u16(data, 0 + 2);
    while (hasBytes(data, offset, 8)) {
        if (!validChunk(data, offset)) {
            break;
        }
        const quint16 type = u16(data, offset);
        const quint32 size = u32(data, offset + 4);
        if (type == RES_TABLE_PACKAGE_TYPE) {
            parsePackage(data, offset, &idToKey, &references);
        }
        offset += int(size);
    }

    foreach (const RefValue &ref, references) {
        const QString source = keyForId(ref.sourceId, idToKey);
        const QString target = keyForId(ref.targetId, idToKey);
        if (!source.isEmpty() && !target.isEmpty()) {
            aliases.insert(source, "@" + target);
        }
    }
    return aliases;
}
